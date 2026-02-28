#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize BLE CLI service
 *
 * Starts NimBLE stack and registers GATT service with CLI command
 * and response characteristics. Advertises as "MimiClaw".
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_cli_init(void);

/**
 * @brief Send response via BLE notification
 *
 * Sends response string to connected BLE client via the response
 * characteristic notification.
 *
 * @param resp Response string to send
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_cli_send_response(const char *resp);

/**
 * @brief Check if BLE client is connected
 *
 * @return true if client connected, false otherwise
 */
bool ble_cli_is_connected(void);
