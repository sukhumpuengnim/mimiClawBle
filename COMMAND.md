# MimiClaw Command Reference

คำสั่งทั้งหมดสำหรับ Serial CLI และ BLE CLI

---

## WiFi Commands

| Command | Description | Example |
|---------|-------------|---------|
| `set_wifi <ssid> <pass>` | ตั้งค่า WiFi credentials (ต้อง restart หลังตั้งค่า) | `set_wifi MyHome mypassword123` |
| `wifi_status` | แสดงสถานะการเชื่อมต่อ WiFi | `wifi_status` |
| `wifi_scan` | สแกนหา WiFi ใกล้เคียง | `wifi_scan` |

---

## LLM API Commands

| Command | Description | Example |
|---------|-------------|---------|
| `set_api_key <key>` | ตั้งค่า API key (Anthropic/OpenAI) | `set_api_key sk-ant-xxxxx` |
| `set_model <model>` | ตั้งค่าชื่อโมเดล | `set_model glm-5` |
| `set_model_provider <provider>` | ตั้งค่า provider (`anthropic` หรือ `openai`) | `set_model_provider openai` |
| `set_api_url <url>` | ตั้งค่า API endpoint URL | `set_api_url https://api.z.ai/api/anthropic/v1/messages` |

### ตัวอย่างการใช้งาน API URL

```bash
# z.ai proxy (default)
set_api_url https://api.z.ai/api/anthropic/v1/messages

# Official Anthropic API
set_api_url https://api.anthropic.com/v1/messages

# Official OpenAI API
set_api_url https://api.openai.com/v1/chat/completions

# GLM via z.ai
set_api_url https://api.z.ai/api/anthropic/v1/messages
set_model_provider openai
set_model glm-5
```

---

## Telegram Commands

| Command | Description | Example |
|---------|-------------|---------|
| `set_tg_token <token>` | ตั้งค่า Telegram bot token | `set_tg_token 123456:ABC-DEF...` |

---

## Proxy Commands

| Command | Description | Example |
|---------|-------------|---------|
| `set_proxy <host> <port> [type]` | ตั้งค่า proxy (type: `http` หรือ `socks5`, default: `http`) | `set_proxy 192.168.1.83 7897 http` |
| `clear_proxy` | ลบการตั้งค่า proxy | `clear_proxy` |

---

## Search Commands

| Command | Description | Example |
|---------|-------------|---------|
| `set_search_key <key>` | ตั้งค่า Brave Search API key สำหรับ web_search tool | `set_search_key BRAVE_API_KEY` |

---

## Memory Commands

| Command | Description | Example |
|---------|-------------|---------|
| `memory_read` | อ่านไฟล์ MEMORY.md | `memory_read` |
| `memory_write <content>` | เขียนข้อความลง MEMORY.md | `memory_write Remember to check emails daily` |

---

## Soul Commands

| Command | Description | Example |
|---------|-------------|---------|
| `soul_read` | อ่านไฟล์ SOUL.md (บุคลิกภาพ AI) | `soul_read` |
| `soul_write <content>` | เขียนข้อความลง SOUL.md | `soul_write I am MimiClaw, a helpful assistant` |

---

## User Commands

| Command | Description | Example |
|---------|-------------|---------|
| `user_read` | อ่านไฟล์ USER.md (ข้อมูลผู้ใช้) | `user_read` |
| `user_write <content>` | เขียนข้อความลง USER.md | `user_write Name: ลูกพี่, Language: Thai` |

---

## Session Commands

| Command | Description | Example |
|---------|-------------|---------|
| `session_list` | แสดงรายการ session ทั้งหมด | `session_list` |
| `session_clear <chat_id>` | ลบ session ตาม chat_id | `session_clear tg_123456789` |

---

## Skills Commands

| Command | Description | Example |
|---------|-------------|---------|
| `skill_list` | แสดงรายการ skills ทั้งหมด | `skill_list` |
| `skill_show <name>` | แสดงเนื้อหา skill (มีหรือไม่มี .md ก็ได้) | `skill_show weather` |
| `skill_search <keyword>` | ค้นหา skill ตาม keyword | `skill_search translate` |

---

## Configuration Commands

| Command | Description | Example |
|---------|-------------|---------|
| `config_show` | แสดงการตั้งค่าปัจจุบันทั้งหมด | `config_show` |
| `config_reset` | ลบการตั้งค่าทั้งหมดใน NVS (กลับไปใช้ build-time defaults) | `config_reset` |

---

## System Commands

| Command | Description | Example |
|---------|-------------|---------|
| `heap_info` | แสดงหน่วยความจำที่เหลืออยู่ | `heap_info` |
| `heartbeat_trigger` | กระตุ้น heartbeat check | `heartbeat_trigger` |
| `cron_start` | เริ่ม cron scheduler | `cron_start` |
| `tool_exec <name> [json]` | รัน tool โดยตรง | `tool_exec web_search '{"query":"weather bangkok"}'` |
| `restart` | รีสตาร์ทอุปกรณ์ | `restart` |
| `help` | แสดงรายการคำสั่ง (BLE CLI เท่านั้น) | `help` |

---

## ตัวอย่างการตั้งค่าครบชุด

### ใช้ z.ai proxy + GLM-5

```bash
set_api_key YOUR_ZAI_API_KEY
set_model_provider openai
set_model glm-5
set_api_url https://api.z.ai/api/anthropic/v1/messages
set_proxy 192.168.1.83 7897 http
config_show
restart
```

### ใช้ Anthropic API โดยตรง

```bash
set_api_key sk-ant-xxxxx
set_model_provider anthropic
set_model claude-opus-4-5
set_api_url https://api.anthropic.com/v1/messages
clear_proxy
config_show
restart
```

### ใช้ OpenAI API โดยตรง

```bash
set_api_key sk-xxxxx
set_model_provider openai
set_model gpt-4o
set_api_url https://api.openai.com/v1/chat/completions
clear_proxy
config_show
restart
```

---

## หมายเหตุ

- การตั้งค่าทั้งหมดถูกเก็บใน NVS และจะยังอยู่หลัง restart
- คำสั่ง `set_wifi`, `set_proxy`, `clear_proxy` ต้อง restart ก่อนจะมีผล
- คำสั่งอื่นๆ มีผลทันทีโดยไม่ต้อง restart
- ใช้ `config_show` เพื่อตรวจสอบการตั้งค่าปัจจุบัน
