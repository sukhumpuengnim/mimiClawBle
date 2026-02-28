# MimiClaw History

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
