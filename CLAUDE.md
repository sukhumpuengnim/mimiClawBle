# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MimiClaw is an AI assistant firmware for ESP32-S3 (16MB flash, 8MB PSRAM) — a pure C/FreeRTOS implementation with no Linux or Node.js. It connects to Telegram for messaging and uses Claude or GPT APIs for the agent loop with tool calling support.

## Build Commands

```bash
# Set target (first time only)
idf.py set-target esp32s3

# Build
idf.py build

# Clean build (required after any mimi_secrets.h change)
idf.py fullclean && idf.py build

# Flash and monitor
idf.py -p PORT flash monitor

# Common ports: /dev/ttyACM0 (Linux), /dev/cu.usbmodem* (macOS)
```

> **USB Port Warning**: Use the USB port labeled "USB" (native USB Serial/JTAG), not "COM" (external UART bridge).

## Configuration

Two-layer system: build-time defaults in `main/mimi_secrets.h` with runtime overrides via serial CLI (stored in NVS).

1. Copy template: `cp main/mimi_secrets.h.example main/mimi_secrets.h`
2. Edit credentials (WiFi, Telegram token, API key)
3. CLI overrides persist in NVS and take priority over build-time values

Key defines in `main/mimi_config.h`:
- `MIMI_SECRET_WIFI_SSID/PASS` — WiFi credentials
- `MIMI_SECRET_TG_TOKEN` — Telegram bot token
- `MIMI_SECRET_API_KEY` — Anthropic or OpenAI API key
- `MIMI_SECRET_MODEL_PROVIDER` — "anthropic" or "openai"
- `MIMI_SECRET_MODEL` — Model ID (default: claude-opus-4-5)

## Architecture

Dual-core FreeRTOS design:
- **Core 0**: I/O tasks (Telegram polling, WebSocket server, serial CLI, WiFi events, outbound dispatch)
- **Core 1**: Agent loop (context building, LLM API calls, tool execution)

### Message Flow

```
User → Telegram/WebSocket → Inbound Queue → Agent Loop (Core 1)
  → Context Builder (load SOUL.md + USER.md + MEMORY.md + history)
  → LLM API call with tools
  → [ReAct loop: tool_use → execute tool → send results → repeat] (max 10 iterations)
  → Outbound Queue → Dispatch → Telegram/WebSocket
```

### Key Modules

| Directory | Purpose |
|-----------|---------|
| `main/agent/` | ReAct agent loop, context builder |
| `main/llm/` | LLM API proxy (Anthropic/OpenAI), tool_use parsing |
| `main/tools/` | Tool registry, web_search, cron tools |
| `main/memory/` | MEMORY.md + daily notes, JSONL session files |
| `main/telegram/` | Bot long polling, message sending |
| `main/gateway/` | WebSocket server on port 18789 |
| `main/bus/` | FreeRTOS message queues (inbound/outbound) |
| `main/cli/` | Serial console REPL |

### Storage (SPIFFS, 12MB partition)

```
/spiffs/config/SOUL.md      — AI personality
/spiffs/config/USER.md      — User profile
/spiffs/memory/MEMORY.md    — Long-term memory
/spiffs/memory/YYYY-MM-DD.md — Daily notes
/spiffs/sessions/tg_<id>.jsonl — Chat history
/spiffs/cron.json           — Scheduled jobs
/spiffs/HEARTBEAT.md        — Autonomous task list
```

## Serial CLI Commands

Runtime config (saved to NVS):
```
wifi_set <ssid> <pass>    set_tg_token <token>    set_api_key <key>
set_model_provider <anthropic|openai>    set_model <model_id>
set_proxy <host> <port>  clear_proxy   set_search_key <key>
config_show              config_reset
```

Debug:
```
wifi_status   memory_read   memory_write <content>   heap_info
session_list  session_clear <chat_id>   heartbeat_trigger
cron_start    restart
```

## Tool System

ReAct agent loop with tool calling. Tools defined in `main/tools/`:
- `web_search` — Brave Search API
- `cron_add/cron_list/cron_remove` — Scheduled jobs
- `get_current_time` — Fetch time via HTTP

Tools are registered at startup via `tool_registry_init()` and exposed to the LLM with JSON schemas.

## Code Conventions

- ESP-IDF v5.5+
- Large buffers (32KB+) allocated from PSRAM via `heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM)`
- Commit style: `docs:`, `fix:`, `feat:`
- Match existing naming and formatting
- No external peripheral changes (see CONTRIBUTING.md)

## Reference Docs

- `docs/ARCHITECTURE.md` — Full system design, memory budget, flash partitions
- `docs/TODO.md` — Feature gap tracker vs Nanobot reference
- `CONTRIBUTING.md` — Contribution guidelines
- https://github.com/memovai/mimiclaw
