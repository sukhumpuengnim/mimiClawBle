# MimiClaw History

## 2026-02-28

### เพิ่มคำสั่ง soul_read, soul_write, user_read, user_write

**เพิ่มคำสั่ง CLI สำหรับอ่านและเขียน SOUL.md และ USER.md แบบ runtime**

การเปลี่ยนแปลง:
- `main/memory/memory_store.h` - เพิ่ม `memory_read_soul()`, `memory_write_soul()`, `memory_read_user()`, `memory_write_user()`
- `main/memory/memory_store.c` - เพิ่ม implementation สำหรับ soul และ user read/write
- `main/cli/serial_cli.c` - เพิ่มคำสั่ง `soul_read`, `soul_write`, `user_read`, `user_write`
- `main/ble/ble_cli.c` - เพิ่มคำสั่งเดียวกันใน BLE CLI
- `COMMAND.md` - เพิ่ม documentation สำหรับ Soul Commands และ User Commands

การใช้งาน:
```
mimi> soul_read                              # ดูบุคลิกภาพ AI
mimi> soul_write I am MimiClaw, friendly...  # เปลี่ยนบุคลิกภาพ AI
mimi> user_read                              # ดูข้อมูลผู้ใช้
mimi> user_write Name: ลูกพี่, Language: Thai  # เปลี่ยนข้อมูลผู้ใช้
```

### เพิ่มคำสั่ง set_api_url สำหรับกำหนด API URL แบบ runtime

**เพิ่มคำสั่ง `set_api_url` ให้สามารถเปลี่ยน API endpoint ได้โดยไม่ต้อง flash ใหม่**

การเปลี่ยนแปลง:
- `main/mimi_config.h` - เพิ่ม NVS key `MIMI_NVS_KEY_API_URL`
- `main/llm/llm_proxy.h` - เพิ่ม `llm_set_api_url()` และ `llm_get_api_url()`
- `main/llm/llm_proxy.c` - เก็บ API URL ใน NVS, fallback ไป default ตาม provider
- `main/cli/serial_cli.c` - เพิ่มคำสั่ง `set_api_url <url>`
- `main/ble/ble_cli.c` - เพิ่มคำสั่ง `set_api_url` ใน BLE CLI

การใช้งาน:
```
mimi> set_api_url https://api.anthropic.com/v1/messages
mimi> set_api_url https://api.z.ai/api/anthropic/v1/messages
mimi> set_api_url https://api.openai.com/v1/chat/completions
mimi> config_show  # แสดง API URL ปัจจุบัน
```

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

### การใช้งาน PSRAM

**MimiClaw ใช้ PSRAM 8MB (Octal) สำหรับ large allocations**

Configuration ใน `sdkconfig.defaults.esp32s3`:
```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
```

การจัดการหน่วยความจำ:
- Large buffers (32KB+) ควร allocate จาก PSRAM: `heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM)`
- ESP-IDF จัดการ PSRAM allocation อัตโนมัติสำหรับ large allocations

การใช้งาน PSRAM ในระบบ:
| Component | Memory Usage |
|-----------|--------------|
| LLM stream buffer | 32 KB |
| BLE stack (NimBLE) | ~60-80 KB |
| Session data | ~1 KB/session |
| SPIFFS cache | 4 KB |

Memory Budget สำหรับ BLE CLI:
- NimBLE stack: ~50-70 KB
- BLE task stack: 6 KB
- Buffers: ~1 KB
- **รวม: ~60-80 KB** จาก 8MB PSRAM (ผลกระทบน้อยมาก)

ผลกระทบต่อหน่วยความจำ:
- ขนาด firmware: ~1.37 MB (เหลือ 35% ใน partition)
- BLE ใช้เพิ่ม ~85 KB จาก PSRAM (ไม่กระทบ internal RAM)

### ESP-IDF Menuconfig

**ใช้สำหรับตั้งค่า firmware configuration ตอน build**

เรียกใช้งาน:
```bash
source ~/.espressif/esp-idf-v5.5.2/export.sh
idf.py menuconfig
```

Keyboard shortcuts:
| Key | ฟังก์ชัน |
|-----|----------|
| `↑` `↓` | เลื่อนขึ้นลง |
| `Enter` | เข้าเมนู / เลือก option |
| `Esc` | ออกจากเมนู |
| `Space` | เลือก/ไม่เลือก option |
| `?` | ดู help |
| `/` | ค้นหา option |
| `S` | Save |
| `Q` | ออก |

เมนูสำคัญสำหรับ MimiClaw:

**1. PSRAM (8MB Octal)**
```
Component config → ESP PSRAM
├── [*] Support for external SPI-connected RAM
├── SPI RAM Mode (Octal)
├── SPI RAM Speed (80MHz)
├── SPI RAM Size (8MB)
└── [*] Initialize SPI RAM during startup
```

**2. BLE (NimBLE)**
```
Component config → Bluetooth
├── [*] Bluetooth
├── Bluetooth Host (NimBLE)
└── NimBLE Options
    ├── [*] Enable BLE 5.0 features
    ├── Maximum number of concurrent connections (1)
    └── Host Task Stack Size (4096)
```

**3. Console (USB Serial/JTAG)**
```
Component config → ESP System Settings
└── Console
    └── Console output (USB Serial/JTAG)
```

**4. Logging**
```
Component config → Log output
├── Default log verbosity (Info)
└── [*] Enable ANSI color codes
```

**5. FreeRTOS**
```
Component config → FreeRTOS
├── [*] Enable FreeRTOS to run on multiple cores
└── Kernel config
    └── Maximum number of tasks (32)
```

ไฟล์ที่เกี่ยวข้อง:
- `sdkconfig.defaults.esp32s3` - Default settings (กำหนดไว้ใน project)
- `sdkconfig` - Current settings (generate จาก menuconfig)
- `build/config/sdkconfig.h` - Header สำหรับ code

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
