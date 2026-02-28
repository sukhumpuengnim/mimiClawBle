# MimiClaw History

## 2026-02-28

### เพิ่มฟีเจอร์ BLE CLI สำหรับการกำหนดค่าแบบไร้สาย

**สร้าง Bluetooth Low Energy CLI เพื่อให้สามารถกำหนดค่า MimiClaw ได้จากโทรศัพท์โดยไม่ต้องใช้สาย USB**

ไฟล์ที่สร้างใหม่ (`main/ble/`):
- `ble_cli.h` - Interface สำหรับ BLE CLI
- `ble_cli.c` - Implementation ด้วย NimBLE stack (~700 บรรทัด)
- `CMakeLists.txt` - Build configuration

การออกแบบ BLE Service:
- ชื่ออุปกรณ์: "MimiClaw"
- Service UUID: 0x1813
- Command Characteristic (0x2A01): รับคำสั่งจากโทรศัพท์ (Write)
- Response Characteristic (0x2A02): ส่งผลลัพธ์ไปโทรศัพท์ (Read + Notify)

คำสั่งที่รองรับบน BLE (เหมือนกับ Serial CLI):
- WiFi: `set_wifi <ssid> <pass>`, `wifi_status`, `wifi_scan`
- API: `set_api_key <key>`, `set_model <model>`, `set_model_provider <provider>`, `set_tg_token <token>`
- Proxy: `set_proxy <host> <port> [type]`, `clear_proxy`, `set_search_key <key>`
- Config: `config_show`, `config_reset`
- Memory: `memory_read`, `memory_write <content>`
- Session: `session_list`, `session_clear <chat_id>`
- Skills: `skill_list`
- System: `heap_info`, `heartbeat_trigger`, `cron_start`, `restart`, `help`

วิธีใช้งาน:
1. เปิดแอป nRF Connect หรือ BLE Terminal บนโทรศัพท์
2. Scan และ connect ไปที่ "MimiClaw"
3. Subscribe ที่ Response characteristic
4. เขียนคำสั่งที่ Command characteristic

ผลกระทบต่อหน่วยความจำ:
- ขนาด firmware: ~1.37 MB (เหลือ 35% ใน partition)
- BLE ใช้เพิ่ม ~85 KB

### เปลี่ยน Console เป็น USB Serial/JTAG

**เปลี่ยน console หลักจาก UART เป็น USB Serial/JTAG**

การเปลี่ยนแปลงใน `sdkconfig.defaults.esp32s3`:
- เดิม: UART เป็น primary, USB Serial/JTAG เป็น secondary
- ใหม่: USB Serial/JTAG เป็น primary (ตัวเดียว)

ข้อดี:
- ใช้ Serial CLI ผ่าน USB port ได้โดยตรง
- ไม่ต้องย้ายสายไปเสียบที่ COM port แล้ว
- Flash และ serial monitor ใช้ USB port เดียวกัน

การเปลี่ยนแปลง logging ใน `main/mimi.c`:
- เงียบช่วง early boot (ลด noise)
- เปิด logging หลัง boot complete
- เพิ่มข้อความ "MimiClaw boot complete - all systems ready"

ผลลัพธ์:
- พิมพ์คำสั่งที่ `mimi>` prompt ได้จาก USB port โดยตรง
- เห็น logs หลังจาก boot เสร็จสมบูรณ์

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
