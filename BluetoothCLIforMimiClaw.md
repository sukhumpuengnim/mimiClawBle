# Plan: Bluetooth CLI for MimiClaw

## Context
User wants to configure MimiClaw via Bluetooth instead of Serial CLI. All commands available through Serial CLI should be accessible via Bluetooth, allowing wireless configuration of WiFi, API keys, model settings, etc.

## Approach
Add a BLE (Bluetooth Low Energy) service that exposes the same CLI commands wirelessly. Using **NimBLE** stack for memory efficiency (~50-70KB RAM vs Bluedroid's 80-120KB).

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                      Core 0                              │
├─────────────────┬─────────────────┬─────────────────────┤
│  Serial CLI     │  BLE CLI        │  Other I/O tasks    │
│  (existing)     │  (new)          │  (Telegram, etc)    │
└────────┬────────┴────────┬────────┴─────────────────────┘
         │                 │
         ▼                 ▼
┌─────────────────────────────────────────────────────────┐
│              Shared Command Handlers                     │
│  (wifi_set, set_api_key, set_model, config_show, etc)   │
└─────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────┐
│                    NVS Storage                           │
└─────────────────────────────────────────────────────────┘
```

## Files to Create/Modify

### New Files
| File | Purpose |
|------|---------|
| `main/ble/ble_cli.h` | BLE CLI interface |
| `main/ble/ble_cli.c` | BLE service implementation |
| `main/ble/CMakeLists.txt` | BLE component build config |

### Modified Files
| File | Changes |
|------|---------|
| `main/CMakeLists.txt` | Add ble component |
| `main/mimi_config.h` | Add BLE config defines |
| `main/mimi.c` | Initialize BLE in app_main() |
| `sdkconfig.defaults.esp32s3` | Enable BT configs |

## Implementation Steps

### Step 1: Enable BLE in sdkconfig
Add to `sdkconfig.defaults.esp32s3`:
```
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=4096
```

### Step 2: Create BLE Component Structure
```
main/ble/
├── CMakeLists.txt
├── ble_cli.h
└── ble_cli.c
```

### Step 3: BLE Service Design
- **Service UUID**: Custom (0x1813 or generate unique)
- **Characteristics**:
  - Command TX (write): Receive commands from phone
  - Response RX (notify): Send responses to phone
  - Status (read): Connection/device status

### Step 4: Command Processing Flow
1. Phone app writes command string to TX characteristic
2. BLE task receives write event
3. Parse command using same argtable3 logic as Serial CLI
4. Execute command (reuse existing handlers)
5. Send response via notification

### Step 5: Configuration Defines (mimi_config.h)
```c
/* BLE CLI */
#define MIMI_BLE_DEVICE_NAME        "MimiClaw"
#define MIMI_BLE_STACK              (6 * 1024)
#define MIMI_BLE_PRIO               3
#define MIMI_BLE_CORE               0
#define MIMI_BLE_CMD_MAX_LEN        256
#define MIMI_BLE_RESP_MAX_LEN       512
```

### Step 6: Key Functions to Implement
```c
// Initialization
esp_err_t ble_cli_init(void);

// Command handling (reuse Serial CLI handlers)
// - All cmd_* functions already exist in serial_cli.c
// - BLE just calls them with parsed arguments

// BLE callbacks
static int ble_cmd_write_cb(...);  // Receive command
static void ble_notify_response(const char *resp);  // Send response
```

## Reusing Serial CLI Commands
The Serial CLI already has all command handlers implemented. BLE CLI will:
1. Parse incoming BLE data as command string
2. Call the same underlying functions (or refactor to shared module)

Option: Extract command handlers to shared `main/cli/cli_commands.c` that both Serial and BLE can use.

## Mobile App Options

### Option 1: ESP Provisioning (Recommended for Initial Setup)
Espressif's official app for device provisioning - good for first-time setup.

| Platform | App Name | Link |
|----------|----------|------|
| Android | ESP BLE Provisioning | [Play Store](https://play.google.com/store/apps/details?id=com.espressif.provble) |
| iOS | ESP Provisioning | [App Store](https://apps.apple.com/app/esp-ble-provisioning/id1477340066) |

**Pros:**
- Official app, ready to use
- Nice UI
- Built-in security (encryption)
- WiFi provisioning ready

**Cons:**
- Designed for provisioning only (initial setup)
- Uses Espressif's protocol (Protobuf/JSON)
- Cannot run arbitrary CLI commands

### Option 2: BLE Terminal Apps (For Full CLI)
Generic BLE terminal apps - work like serial terminal over Bluetooth.

| Platform | App Name | Ease of Use | Link |
|----------|----------|-------------|------|
| Android | **Serial Bluetooth Terminal** | ⭐ Easiest | [Play Store](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal) |
| Android | BLE Terminal | Medium | [Play Store](https://play.google.com/store/apps/details?id=com.blutus.connectble) |
| Android | nRF Connect | Advanced | [Play Store](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp) |
| iOS | nRF Connect | Standard | [App Store](https://apps.apple.com/app/nrf-connect/id1054362403) |
| iOS | BLE Terminal | Medium | [App Store](https://apps.apple.com/app/ble-terminal/id1450231355) |

**Pros:**
- Full CLI access (like Serial CLI)
- Can run any command at any time
- Can check status, debug, change settings

**Cons:**
- No built-in encryption
- Need to know CLI commands

### Feature Comparison

| Feature | ESP Provisioning | BLE Terminal |
|---------|------------------|--------------|
| WiFi setup | ✅ Built-in | ✅ via `wifi_set` |
| API key | ✅ Custom param | ✅ via `set_api_key` |
| Model/provider | ✅ Custom param | ✅ via `set_model` |
| config_show | ❌ Not supported | ✅ Works |
| heap_info | ❌ Not supported | ✅ Works |
| session_list | ❌ Not supported | ✅ Works |
| Runtime commands | ❌ No | ✅ Yes |
| Security | ✅ Encrypted | ❌ None |
| App ready | ✅ Yes | ✅ Yes |

### Recommendation
- **Use ESP Provisioning** if you only need initial device setup (WiFi + basic config)
- **Use BLE Terminal** if you want full CLI access for debugging and runtime changes

## Testing Plan
1. Build and flash firmware
2. Install chosen app on phone
3. Scan and connect to "MimiClaw" device
4. Test commands:
   - `config_show` - verify response
   - `wifi_set <ssid> <pass>` - test WiFi setup
   - `set_api_key <key>` - test API config
5. Verify settings persist after reboot

## Memory Budget
- NimBLE stack: ~50-70KB
- BLE task stack: 6KB
- Buffers: ~1KB
- **Total**: ~60-80KB from 8MB PSRAM (minimal impact)

## Verification
- [ ] Firmware builds without errors
- [ ] BLE advertises as "MimiClaw"
- [ ] Can connect with phone
- [ ] Can send `config_show` and receive response
- [ ] Can change WiFi settings via BLE
- [ ] Can change API key via BLE
- [ ] Settings persist after reboot
