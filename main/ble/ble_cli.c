#include "ble_cli.h"
#include "mimi_config.h"
#include "wifi/wifi_manager.h"
#include "telegram/telegram_bot.h"
#include "llm/llm_proxy.h"
#include "memory/memory_store.h"
#include "memory/session_mgr.h"
#include "proxy/http_proxy.h"
#include "tools/tool_registry.h"
#include "tools/tool_web_search.h"
#include "cron/cron_service.h"
#include "heartbeat/heartbeat.h"
#include "skills/skill_loader.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble_cli";

/* Forward declaration */
static void ble_cli_start_advertising(void);

/* Connection state */
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool subscribed = false;

/* Response buffer */
static char response_buf[MIMI_BLE_RESP_MAX_LEN];
static size_t response_len = 0;

/* GATT service handles */
static const ble_uuid16_t svc_uuid = BLE_UUID16_INIT(MIMI_BLE_SVC_UUID);
static const ble_uuid16_t cmd_char_uuid = BLE_UUID16_INIT(MIMI_BLE_CHAR_CMD_UUID);
static const ble_uuid16_t resp_char_uuid = BLE_UUID16_INIT(MIMI_BLE_CHAR_RESP_UUID);

static uint16_t resp_val_handle;

/* --- Helper: append to response buffer --- */
static void resp_append(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    size_t remaining = sizeof(response_buf) - response_len;
    if (remaining > 1) {
        int written = vsnprintf(response_buf + response_len, remaining, fmt, args);
        if (written > 0) {
            response_len += (size_t)written < remaining ? (size_t)written : remaining - 1;
        }
    }

    va_end(args);
}

static void resp_reset(void)
{
    response_len = 0;
    response_buf[0] = '\0';
}

/* --- Shared command implementations (from serial_cli.c) --- */

static void cmd_wifi_set(const char *ssid, const char *pass)
{
    wifi_manager_set_credentials(ssid, pass);
    resp_append("WiFi credentials saved. Restart to apply.\n");
}

static void cmd_wifi_status(void)
{
    resp_append("WiFi connected: %s\n", wifi_manager_is_connected() ? "yes" : "no");
    resp_append("IP: %s\n", wifi_manager_get_ip());
}

static void cmd_set_tg_token(const char *token)
{
    telegram_set_token(token);
    resp_append("Telegram bot token saved.\n");
}

static void cmd_set_api_key(const char *key)
{
    llm_set_api_key(key);
    resp_append("API key saved.\n");
}

static void cmd_set_model(const char *model)
{
    llm_set_model(model);
    resp_append("Model set.\n");
}

static void cmd_set_model_provider(const char *provider)
{
    llm_set_provider(provider);
    resp_append("Model provider set.\n");
}

static void cmd_memory_read(void)
{
    char *buf = malloc(4096);
    if (!buf) {
        resp_append("Out of memory.\n");
        return;
    }
    if (memory_read_long_term(buf, 4096) == ESP_OK && buf[0]) {
        resp_append("=== MEMORY.md ===\n%s\n=================\n", buf);
    } else {
        resp_append("MEMORY.md is empty or not found.\n");
    }
    free(buf);
}

static void cmd_memory_write(const char *content)
{
    memory_write_long_term(content);
    resp_append("MEMORY.md updated.\n");
}

static void cmd_session_list(void)
{
    resp_append("Sessions:\n");
    /* session_list() prints to stdout, need different approach */
    resp_append("(Use serial CLI for full session list)\n");
}

static void cmd_session_clear(const char *chat_id)
{
    if (session_clear(chat_id) == ESP_OK) {
        resp_append("Session cleared.\n");
    } else {
        resp_append("Session not found.\n");
    }
}

static void cmd_heap_info(void)
{
    resp_append("Internal free: %d bytes\n",
           (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    resp_append("PSRAM free:    %d bytes\n",
           (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    resp_append("Total free:    %d bytes\n",
           (int)esp_get_free_heap_size());
}

static void cmd_set_proxy(const char *host, int port, const char *type)
{
    if (strcmp(type, "http") != 0 && strcmp(type, "socks5") != 0) {
        resp_append("Invalid proxy type: %s. Use http or socks5.\n", type);
        return;
    }
    http_proxy_set(host, (uint16_t)port, type);
    resp_append("Proxy set. Restart to apply.\n");
}

static void cmd_clear_proxy(void)
{
    http_proxy_clear();
    resp_append("Proxy cleared. Restart to apply.\n");
}

static void cmd_set_search_key(const char *key)
{
    tool_web_search_set_key(key);
    resp_append("Search API key saved.\n");
}

static void cmd_wifi_scan(void)
{
    resp_append("WiFi scan started. Check serial output for results.\n");
    wifi_manager_scan_and_print();
}

static void cmd_skill_list(void)
{
    char *buf = malloc(4096);
    if (!buf) {
        resp_append("Out of memory.\n");
        return;
    }

    size_t n = skill_loader_build_summary(buf, 4096);
    if (n == 0) {
        resp_append("No skills found.\n");
    } else {
        resp_append("=== Skills ===\n%s", buf);
    }
    free(buf);
}

static void print_config_line(const char *label, const char *ns, const char *key,
                              const char *build_val, bool mask)
{
    char nvs_val[128] = {0};
    const char *source = "not set";
    const char *display = "(empty)";

    nvs_handle_t nvs;
    if (nvs_open(ns, NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(nvs_val);
        if (nvs_get_str(nvs, key, nvs_val, &len) == ESP_OK && nvs_val[0]) {
            source = "NVS";
            display = nvs_val;
        }
        nvs_close(nvs);
    }

    if (strcmp(source, "not set") == 0 && build_val[0] != '\0') {
        source = "build";
        display = build_val;
    }

    if (mask && strlen(display) > 6 && strcmp(display, "(empty)") != 0) {
        resp_append("  %-14s: %.4s****  [%s]\n", label, display, source);
    } else {
        resp_append("  %-14s: %s  [%s]\n", label, display, source);
    }
}

static void cmd_config_show(void)
{
    resp_append("=== Current Configuration ===\n");
    print_config_line("WiFi SSID",  MIMI_NVS_WIFI,   MIMI_NVS_KEY_SSID,     MIMI_SECRET_WIFI_SSID,  false);
    print_config_line("WiFi Pass",  MIMI_NVS_WIFI,   MIMI_NVS_KEY_PASS,     MIMI_SECRET_WIFI_PASS,  true);
    print_config_line("TG Token",   MIMI_NVS_TG,     MIMI_NVS_KEY_TG_TOKEN, MIMI_SECRET_TG_TOKEN,   true);
    print_config_line("API Key",    MIMI_NVS_LLM,    MIMI_NVS_KEY_API_KEY,  MIMI_SECRET_API_KEY,    true);
    print_config_line("Model",      MIMI_NVS_LLM,    MIMI_NVS_KEY_MODEL,    MIMI_SECRET_MODEL,      false);
    print_config_line("Provider",   MIMI_NVS_LLM,    MIMI_NVS_KEY_PROVIDER, MIMI_SECRET_MODEL_PROVIDER, false);
    print_config_line("Proxy Host", MIMI_NVS_PROXY,  MIMI_NVS_KEY_PROXY_HOST, MIMI_SECRET_PROXY_HOST, false);
    print_config_line("Proxy Port", MIMI_NVS_PROXY,  MIMI_NVS_KEY_PROXY_PORT, MIMI_SECRET_PROXY_PORT, false);
    print_config_line("Search Key", MIMI_NVS_SEARCH, MIMI_NVS_KEY_API_KEY,  MIMI_SECRET_SEARCH_KEY, true);
    resp_append("=============================\n");
}

static void cmd_config_reset(void)
{
    const char *namespaces[] = {
        MIMI_NVS_WIFI, MIMI_NVS_TG, MIMI_NVS_LLM, MIMI_NVS_PROXY, MIMI_NVS_SEARCH
    };
    for (int i = 0; i < 5; i++) {
        nvs_handle_t nvs;
        if (nvs_open(namespaces[i], NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_erase_all(nvs);
            nvs_commit(nvs);
            nvs_close(nvs);
        }
    }
    resp_append("All NVS config cleared. Build-time defaults will be used on restart.\n");
}

static void cmd_heartbeat_trigger(void)
{
    resp_append("Checking HEARTBEAT.md...\n");
    if (heartbeat_trigger()) {
        resp_append("Heartbeat: agent prompted with pending tasks.\n");
    } else {
        resp_append("Heartbeat: no actionable tasks found.\n");
    }
}

static void cmd_cron_start(void)
{
    esp_err_t err = cron_service_start();
    if (err == ESP_OK) {
        resp_append("Cron service started.\n");
    } else {
        resp_append("Failed to start cron service: %s\n", esp_err_to_name(err));
    }
}

static void cmd_restart(void)
{
    resp_append("Restarting...\n");
    ble_cli_send_response(response_buf);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

static void cmd_help(void)
{
    resp_append("Available commands:\n");
    resp_append("  set_wifi <ssid> <pass>   - Set WiFi credentials\n");
    resp_append("  wifi_status              - Show WiFi status\n");
    resp_append("  wifi_scan                - Scan nearby APs\n");
    resp_append("  set_tg_token <token>     - Set Telegram bot token\n");
    resp_append("  set_api_key <key>        - Set LLM API key\n");
    resp_append("  set_model <model>        - Set LLM model\n");
    resp_append("  set_model_provider <p>   - Set provider (anthropic|openai)\n");
    resp_append("  set_proxy <h> <p> [t]    - Set proxy (type: http|socks5)\n");
    resp_append("  clear_proxy              - Clear proxy\n");
    resp_append("  set_search_key <key>     - Set Brave Search API key\n");
    resp_append("  config_show              - Show current config\n");
    resp_append("  config_reset             - Clear NVS config\n");
    resp_append("  memory_read              - Read MEMORY.md\n");
    resp_append("  memory_write <text>      - Write to MEMORY.md\n");
    resp_append("  session_list             - List sessions\n");
    resp_append("  session_clear <id>       - Clear session\n");
    resp_append("  skill_list               - List skills\n");
    resp_append("  heap_info                - Show memory usage\n");
    resp_append("  heartbeat_trigger        - Trigger heartbeat\n");
    resp_append("  cron_start               - Start cron service\n");
    resp_append("  restart                  - Restart device\n");
    resp_append("  help                     - Show this help\n");
}

/* --- Command parser and dispatcher --- */
static void process_command(const char *cmd, size_t len)
{
    resp_reset();

    /* Make null-terminated copy */
    char *buf = malloc(len + 1);
    if (!buf) {
        resp_append("Out of memory.\n");
        return;
    }
    memcpy(buf, cmd, len);
    buf[len] = '\0';

    /* Trim whitespace */
    char *p = buf;
    while (*p && isspace((unsigned char)*p)) p++;
    char *end = p + strlen(p) - 1;
    while (end > p && isspace((unsigned char)*end)) *end-- = '\0';

    if (*p == '\0') {
        free(buf);
        return;  /* Empty command */
    }

    ESP_LOGI(TAG, "BLE cmd: %s", p);

    /* Simple command parsing */
    char *saveptr;
    char *token = strtok_r(p, " \t", &saveptr);

    if (strcmp(token, "help") == 0) {
        cmd_help();
    }
    else if (strcmp(token, "set_wifi") == 0) {
        char *ssid = strtok_r(NULL, " \t", &saveptr);
        char *pass = strtok_r(NULL, "", &saveptr);  /* Rest is password */
        if (ssid && pass) {
            /* Trim password */
            while (*pass && isspace((unsigned char)*pass)) pass++;
            cmd_wifi_set(ssid, pass);
        } else {
            resp_append("Usage: set_wifi <ssid> <password>\n");
        }
    }
    else if (strcmp(token, "wifi_status") == 0) {
        cmd_wifi_status();
    }
    else if (strcmp(token, "wifi_scan") == 0) {
        cmd_wifi_scan();
    }
    else if (strcmp(token, "set_tg_token") == 0) {
        char *token_val = strtok_r(NULL, "", &saveptr);
        if (token_val) {
            while (*token_val && isspace((unsigned char)*token_val)) token_val++;
            cmd_set_tg_token(token_val);
        } else {
            resp_append("Usage: set_tg_token <token>\n");
        }
    }
    else if (strcmp(token, "set_api_key") == 0) {
        char *key = strtok_r(NULL, "", &saveptr);
        if (key) {
            while (*key && isspace((unsigned char)*key)) key++;
            cmd_set_api_key(key);
        } else {
            resp_append("Usage: set_api_key <key>\n");
        }
    }
    else if (strcmp(token, "set_model") == 0) {
        char *model = strtok_r(NULL, " \t", &saveptr);
        if (model) {
            cmd_set_model(model);
        } else {
            resp_append("Usage: set_model <model>\n");
        }
    }
    else if (strcmp(token, "set_model_provider") == 0) {
        char *provider = strtok_r(NULL, " \t", &saveptr);
        if (provider) {
            cmd_set_model_provider(provider);
        } else {
            resp_append("Usage: set_model_provider <anthropic|openai>\n");
        }
    }
    else if (strcmp(token, "set_proxy") == 0) {
        char *host = strtok_r(NULL, " \t", &saveptr);
        char *port_str = strtok_r(NULL, " \t", &saveptr);
        char *type = strtok_r(NULL, " \t", &saveptr);
        if (host && port_str) {
            int port = atoi(port_str);
            if (!type) type = "http";
            cmd_set_proxy(host, port, type);
        } else {
            resp_append("Usage: set_proxy <host> <port> [http|socks5]\n");
        }
    }
    else if (strcmp(token, "clear_proxy") == 0) {
        cmd_clear_proxy();
    }
    else if (strcmp(token, "set_search_key") == 0) {
        char *key = strtok_r(NULL, "", &saveptr);
        if (key) {
            while (*key && isspace((unsigned char)*key)) key++;
            cmd_set_search_key(key);
        } else {
            resp_append("Usage: set_search_key <key>\n");
        }
    }
    else if (strcmp(token, "config_show") == 0) {
        cmd_config_show();
    }
    else if (strcmp(token, "config_reset") == 0) {
        cmd_config_reset();
    }
    else if (strcmp(token, "memory_read") == 0) {
        cmd_memory_read();
    }
    else if (strcmp(token, "memory_write") == 0) {
        char *content = strtok_r(NULL, "", &saveptr);
        if (content) {
            while (*content && isspace((unsigned char)*content)) content++;
            cmd_memory_write(content);
        } else {
            resp_append("Usage: memory_write <content>\n");
        }
    }
    else if (strcmp(token, "session_list") == 0) {
        cmd_session_list();
    }
    else if (strcmp(token, "session_clear") == 0) {
        char *chat_id = strtok_r(NULL, " \t", &saveptr);
        if (chat_id) {
            cmd_session_clear(chat_id);
        } else {
            resp_append("Usage: session_clear <chat_id>\n");
        }
    }
    else if (strcmp(token, "skill_list") == 0) {
        cmd_skill_list();
    }
    else if (strcmp(token, "heap_info") == 0) {
        cmd_heap_info();
    }
    else if (strcmp(token, "heartbeat_trigger") == 0) {
        cmd_heartbeat_trigger();
    }
    else if (strcmp(token, "cron_start") == 0) {
        cmd_cron_start();
    }
    else if (strcmp(token, "restart") == 0) {
        free(buf);
        cmd_restart();
        return;  /* Won't reach here */
    }
    else {
        resp_append("Unknown command: %s\nType 'help' for available commands.\n", token);
    }

    free(buf);
    ble_cli_send_response(response_buf);
}

/* --- GATT access callbacks --- */

static int cmd_char_write_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->om->om_len > 0) {
        process_command((const char *)ctxt->om->om_data, ctxt->om->om_len);
    }

    return 0;
}

static int resp_char_read_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    /* Return current response buffer */
    int rc = os_mbuf_append(ctxt->om, response_buf, response_len);
    return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

/* --- GATT service definition --- */

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &cmd_char_uuid.u,
                .access_cb = cmd_char_write_cb,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                .uuid = &resp_char_uuid.u,
                .access_cb = resp_char_read_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &resp_val_handle,
            },
            {
                0,  /* No more characteristics */
            },
        },
    },
    {
        0,  /* No more services */
    },
};

/* --- BLE host callbacks --- */

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "BLE connect; status=%d", event->connect.status);
        if (event->connect.status == 0) {
            conn_handle = event->connect.conn_handle;
        } else {
            conn_handle = BLE_HS_CONN_HANDLE_NONE;
            subscribed = false;
            /* Restart advertising */
            ble_cli_start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnect; reason=%d", event->disconnect.reason);
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        subscribed = false;
        ble_cli_start_advertising();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "BLE subscribe; attr_handle=%d", event->subscribe.attr_handle);
        if (event->subscribe.attr_handle == resp_val_handle) {
            subscribed = event->subscribe.cur_notify;
        }
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "BLE advertise complete");
        ble_cli_start_advertising();
        break;

    default:
        break;
    }

    return 0;
}

/* --- Public API --- */

bool ble_cli_is_connected(void)
{
    return conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

esp_err_t ble_cli_send_response(const char *resp)
{
    if (!subscribed || conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return ESP_ERR_INVALID_STATE;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(resp, strlen(resp));
    if (!om) {
        return ESP_ERR_NO_MEM;
    }

    int rc = ble_gattc_notify_custom(conn_handle, resp_val_handle, om);
    return (rc == 0) ? ESP_OK : ESP_FAIL;
}

void ble_cli_start_advertising(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    /* Set advertising data */
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)MIMI_BLE_DEVICE_NAME;
    fields.name_len = strlen(MIMI_BLE_DEVICE_NAME);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set adv fields: %d", rc);
        return;
    }

    /* Start advertising */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising: %d", rc);
    } else {
        ESP_LOGI(TAG, "BLE advertising started as '%s'", MIMI_BLE_DEVICE_NAME);
    }
}

static void ble_on_sync(void)
{
    ESP_LOGI(TAG, "BLE sync");
    ble_cli_start_advertising();
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE reset; reason=%d", reason);
}

static void ble_host_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_cli_init(void)
{
    int rc;

    /* Initialize NimBLE */
    rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NimBLE: %d", rc);
        return rc;
    }

    /* Configure host callbacks */
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    /* Initialize GAP and GATT services */
    ble_svc_gap_init();
    ble_svc_gatt_init();

    /* Register our GATT service */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT cfg: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT services: %d", rc);
        return ESP_FAIL;
    }

    /* Set device name */
    rc = ble_svc_gap_device_name_set(MIMI_BLE_DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set device name: %d", rc);
        return ESP_FAIL;
    }

    /* Start BLE host task */
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE CLI initialized");
    return ESP_OK;
}
