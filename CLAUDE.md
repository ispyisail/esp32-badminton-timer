# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-based badminton court timer with a web UI served over WebSocket. The ESP32 is the single source of truth for all state (timer, users, cached bookings). Browser clients connect via WebSocket for real-time bidirectional sync. Integrates with Hello Club API for fetching court bookings.

**Key directories:** `src/` (firmware), `data/` (web UI served from SPIFFS), `test-server/` (dev mock server), `tests/` (Jest unit and integration tests).

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

The test server (`test-server/server.js`) mocks the entire ESP32 WebSocket API including auth, settings, and Hello Club events. Use it for UI development.

## Tests

```bash
cd tests
npm install
npm test                                     # all suites
npm run test:unit                            # unit only
npm run test:integration                     # spawns test-server on port 18080
npx jest unit/timer-state-machine.test.js    # single file
```

Jest exercises the JS UI logic and the mock server — it never builds or runs the firmware, so C++ changes are not covered by it. Verify those with `pio run`.

**The integration suite is flaky.** All integration suites share one mock-server instance on port 18080 whose timer and user state is never reset between them, so results depend on execution order and timing — identical back-to-back runs of the same command have produced anywhere from 0 to 4 failures, and `--runInBand` does not fix it. Any individual suite run on its own (`npx jest --selectProjects integration --testPathPattern=timer-sync`) passes reliably.

Treat a failure here as unproven until you reproduce it in isolation. Adding or removing a test shifts the ordering and will change which unrelated tests fail, so do not read a changed failure list as a regression in what you just edited.

## Architecture

**Backend (C++ / Arduino on ESP32):**
- `src/main.cpp` — Entry point, WiFi setup, WebSocket server, message routing, auth enforcement, rate limiting
- `src/timer.cpp/h` — Timer state machine: IDLE → RUNNING → PAUSED → FINISHED, with rounds and breaks
- `src/siren.cpp/h` — Non-blocking relay control via state machine (no `delay()` calls anywhere)
- `src/users.cpp/h` — Three-tier auth: VIEWER (no auth) → OPERATOR → ADMIN, SHA-256 password hashing
- `src/helloclub.cpp/h` — Hello Club external API client with retry/backoff. This is the only source of scheduled starts: bookings are polled hourly, cached in NVS, and auto-trigger the timer when their start time arrives. There is no separate schedule module — the trigger check and mid-event boot recovery live in `main.cpp`'s loop.
- `src/settings.cpp/h` — NVS (Preferences) persistence layer
- `src/remotelog.cpp/h` — In-memory diagnostic ring buffer, exposed over `/diag` and the `get_remote_log` action (both admin-only)
- `src/config.h` — All constants, limits, feature flags, pin assignments

**Frontend (vanilla JS, served from SPIFFS):**
- `data/index.html` — UI structure
- `data/script.js` — WebSocket client, 60fps timer interpolation between 5-second server syncs
- `data/style.css` — Responsive CSS
- `data/qrcode.min.js` — QR code generation library
- `data/qr-test.html` — QR code test/demo page

## Key Design Constraints

- **No blocking calls in `loop()`**: Runtime paths are non-blocking — use millis()-based state machines, never `delay()`. The exceptions are deliberate and confined to code that runs before the server is serving or on a path that ends in a reboot: WiFi connect, the captive portal, and factory reset. Do not add `delay()` anywhere else.
- **ESP32 memory**: JSON documents are size-constrained (see `JSON_DOC_SIZE_*` in config.h). Flash is the binding limit — the build sits at ~91% of the default 1.31 MB app partition, so a sizeable feature will need a partition scheme change.
- **NVS storage**: Settings, users, the Hello Club event cache, and captive-portal WiFi credentials persist in NVS (Preferences library). Max 10 operators, 20 cached events.
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

Messages are JSON. See `API.md` for the full reference.

- **Client → server** messages carry an `action`: `start`, `pause`, `reset`, `pause_after_next`, `authenticate`, `save_settings`, `set_timezone`, `add_operator`, `remove_operator`, `get_operators`, `change_password`, `factory_reset`, `get_upcoming_events`, `helloclub_refresh`, `get_helloclub_settings`, `save_helloclub_settings`, `get_qr_config`, `save_qr_settings`, `get_remote_log`.
- **Server → client** messages carry an `event`: `state`, `sync`, `settings`, `start`, `pause`, `resume`, `new_round`, `auth_success`, `viewer_mode`, `error`, `upcoming_events`, `event_auto_started`, `ntp_status`, and others.

Auth state is tracked per-client on the server side. Permission tiers are enforced by the `needsOperator` / `needsAdmin` lists in `handleWebSocketMessage()` — adding an action means adding it to the right list.
