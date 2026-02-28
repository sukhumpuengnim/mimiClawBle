# MimiClaw History

## 2026-02-28

### BLE CLI Feature Implementation

**Added Bluetooth Low Energy CLI for wireless configuration**

- Created new BLE component (`main/ble/`):
  - `ble_cli.h` - BLE CLI interface
  - `ble_cli.c` - NimBLE-based BLE service implementation (~700 lines)
  - `CMakeLists.txt` - Build configuration

- BLE Service Design:
  - Device Name: "MimiClaw"
  - Service UUID: 0x1813 (custom)
  - Command Characteristic (0x2A01): Write-only, receive commands from phone
  - Response Characteristic (0x2A02): Read + Notify, send responses to phone

- Supported Commands (same as Serial CLI):
  - WiFi: `set_wifi`, `wifi_status`, `wifi_scan`
  - API: `set_api_key`, `set_model`, `set_model_provider`, `set_tg_token`
  - Proxy: `set_proxy`, `clear_proxy`, `set_search_key`
  - Config: `config_show`, `config_reset`
  - Memory: `memory_read`, `memory_write`
  - Session: `session_list`, `session_clear`
  - Skills: `skill_list`
  - System: `heap_info`, `heartbeat_trigger`, `cron_start`, `restart`, `help`

- Configuration Changes (`sdkconfig.defaults.esp32s3`):
  - Enabled NimBLE Bluetooth stack
  - Added BLE buffer configuration

- Usage:
  - Use nRF Connect or BLE Terminal app on phone
  - Connect to "MimiClaw" device
  - Write commands to Command characteristic
  - Subscribe to Response characteristic for output

### Console Configuration Update

**Changed console from UART to USB Serial/JTAG as primary**

- Changed `sdkconfig.defaults.esp32s3`:
  - From: UART primary + USB Serial/JTAG secondary
  - To: USB Serial/JTAG primary only

- Benefits:
  - Serial CLI now works via USB port directly
  - No need to move cable to COM port for serial commands
  - Flash and serial monitor work on same USB port

- Logging Changes (`main/mimi.c`):
  - Silent during early boot (cleaner startup)
  - Enabled after boot complete (all systems ready)
  - Added boot complete log message

- Memory Impact:
  - Firmware size: ~1.37 MB (35% free space in partition)
  - BLE adds ~85 KB

## 2026-02-27

### Model Configuration Update

**Changed model from `glm-4.5` to `glm-5`**

- Updated `main/mimi_secrets.h`:
  - `MIMI_SECRET_MODEL`: `"glm-4.5"` → `"glm-5"`
  - `MIMI_SECRET_MODEL_PROVIDER`: `"anthropic"`
  - `MIMI_SECRET_API_KEY`: GLM API key via z.ai proxy

- Configuration details:
  - API Endpoint: `https://api.z.ai/api/anthropic/v1/messages`
  - Uses Anthropic-compatible API format through z.ai proxy
  - Telegram Bot: @mimiClaw001_bot

- Build & Flash:
  - Rebuilt firmware successfully (1.17 MB binary)
  - Flashed to ESP32-S3 at `/dev/ttyACM0`
  - SPIFFS partition: 12 MB

### Previous Session Summary (Feb 26-27, 2026)

- Configured MimiClaw with GLM API support
- Set up Telegram bot integration (@mimiClaw001_bot)
- Troubleshooting performed:
  - Changed API endpoint from `open.bigmodel.cn` to `api.z.ai`
  - Fixed Anthropic API path in `llm_proxy.c`
  - Reduced logging verbosity for cleaner serial output
  - Removed startup "MimiClaw ready" message
- Multiple model attempts:
  - `glm-4-flashx` → Error 1211 (model not found)
  - `glm-4-flash` → API quota exhausted
  - `glm-4.5` → Working
  - `glm-5` → Current configuration
