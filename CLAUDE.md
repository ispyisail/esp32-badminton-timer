# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-based badminton court timer with a web UI served over WebSocket. The ESP32 is the single source of truth for all state (timer, users, schedules). Browser clients connect via WebSocket for real-time bidirectional sync.

## Build & Upload Commands (PlatformIO)

```bash
# Build firmware
pio run

# Upload firmware via USB
pio run --target upload

# Upload SPIFFS web interface (data/ folder)
pio run --target uploadfs

# OTA upload (requires device on network)
pio run --target upload --upload-port badminton-timer.local

# Serial monitor (115200 baud)
pio device monitor
```

## Local Development (No Hardware)

```bash
cd test-server
npm install
npm start
# Opens http://localhost:8080 with full WebSocket API mock
```

The test server (`test-server/server.js`) mocks the entire ESP32 WebSocket API including auth, schedules, and settings. Use it for UI development.

## Architecture

**Backend (C++ / Arduino on ESP32):**
- `src/main.cpp` — Entry point, WiFi setup, WebSocket server, message routing, auth enforcement, rate limiting
- `src/timer.cpp/h` — Timer state machine: IDLE → RUNNING → PAUSED → FINISHED, with rounds and breaks
- `src/siren.cpp/h` — Non-blocking relay control via state machine (no `delay()` calls anywhere)
- `src/users.cpp/h` — Three-tier auth: VIEWER (no auth) → OPERATOR → ADMIN, SHA-256 password hashing
- `src/schedule.cpp/h` — Weekly recurring schedules with owner-based permissions, auto-trigger with debounce
- `src/helloclub.cpp/h` — Hello Club external API client with retry/backoff
- `src/settings.cpp/h` — NVS (Preferences) persistence layer
- `src/config.h` — All constants, limits, feature flags, pin assignments

**Frontend (vanilla JS, served from SPIFFS):**
- `data/index.html` — UI structure
- `data/script.js` — WebSocket client, 60fps timer interpolation between 5-second server syncs
- `data/style.css` — Responsive CSS

## Key Design Constraints

- **No blocking calls**: The entire firmware is non-blocking. Never use `delay()`. Use millis()-based state machines.
- **ESP32 memory**: ~60KB RAM available. JSON documents are size-constrained (see `JSON_DOC_SIZE_*` in config.h).
- **NVS storage**: Settings, users, and schedules persist in NVS (Preferences library). Max 50 schedules, 10 operators.
- **WebSocket broadcast**: State changes broadcast to all connected clients simultaneously. Timer syncs every 5 seconds.
- **Server-authoritative**: All validation and permission checks happen on the ESP32, never trust the client.

## Hardware

- **GPIO 26**: Relay/siren output
- **GPIO 0**: Factory reset button (hold 10s)
- Board: ESP32-WROOM-32 dev board
- WiFi credentials: `src/wifi_credentials.h` (git-ignored, must create from template)

## Configuration

All tunable constants are in `src/config.h`. Feature flags (`ENABLE_WATCHDOG`, `ENABLE_OTA`, etc.) and debug mode (`DEBUG_MODE`) are there too. Debug output uses `DEBUG_PRINTLN`/`DEBUG_PRINTF` macros that compile out when `DEBUG_MODE` is 0.

## WebSocket Protocol

Messages are JSON with a `type` field. See `API.md` for the full message reference. The main message types: `timer_state`, `settings`, `auth`, `schedule_*`, `user_*`. Auth state is tracked per-client on the server side.
