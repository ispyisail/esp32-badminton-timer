# ESP32 Badminton Court Timer

A web-controlled court timer built on the ESP32, designed for badminton clubs and sports facilities that share court time. The ESP32 acts as the single source of truth -- all state, authentication, and scheduling logic runs on the device. Browser clients connect over WebSocket for real-time bidirectional sync.

**Repository:** [github.com/ispyisail/esp32-badminton-timer](https://github.com/ispyisail/esp32-badminton-timer)

## Features

- **Web UI** -- Responsive interface served from SPIFFS. No app install needed. Works on desktop, tablet, and mobile.
- **Real-time sync** -- 60fps client-side interpolation between 5-second server syncs. All connected devices stay in lockstep.
- **Three-tier auth** -- Viewer (no login), Operator (timer control, own schedules), Admin (full system access). SHA-256 password hashing.
- **Weekly scheduling** -- Recurring schedules with a visual calendar grid. Each operator manages their own club's schedules.
- **Hello Club integration** -- Auto-start timers from Hello Club booking events. See [Hello Club Integration](#hello-club-integration) below.
- **QR code access** -- Admin can configure a WiFi QR code so guests can scan to join the network and open the timer.
- **Continuous mode** -- Rounds repeat indefinitely until an event cutoff time or manual stop.
- **Pause After Next round** -- One-shot flag: the timer pauses automatically at the end of the current round instead of starting the next.
- **Non-blocking siren** -- Relay-driven siren control via state machine. No `delay()` calls anywhere in the firmware.
- **OTA updates** -- Wireless firmware updates after initial USB flash.
- **Configurable timezone** -- 20 common timezones with automatic DST adjustment.
- **Mid-event boot recovery** -- If the ESP32 reboots during an active Hello Club event, it calculates the correct round and remaining time and resumes automatically.

## Quick Start

### Local Development (No Hardware)

```bash
cd test-server
npm install
npm start
```

Open http://localhost:8080. Login as `admin` / `admin`, or leave fields empty for viewer mode.

### Production Deployment

1. Create `src/wifi_credentials.h` from the example template and set your passwords.
2. Build and upload:
   ```bash
   pio run --target upload       # Firmware
   pio run --target uploadfs     # Web interface (data/ folder)
   ```
3. Connect to the `BadmintonTimerSetup` WiFi network and enter your WiFi credentials.
4. Access the timer at http://badminton-timer.local

## Hardware

| Component | Details |
|-----------|---------|
| Board | ESP32-WROOM-32 dev board |
| Relay output | GPIO 26 -- drives siren/buzzer |
| Factory reset | GPIO 0 (BOOT button) -- hold 10 seconds |
| Power | 5V USB for ESP32; separate supply for siren |

See **INSTALL_GUIDE.md** for full wiring diagram and assembly instructions.

## Hello Club Integration

The timer integrates with the [Hello Club](https://www.helloclub.com/) booking system API to automatically start court timers based on scheduled events.

### How it works

1. Admin enters a Hello Club API key in Settings.
2. The ESP32 polls the Hello Club API (hourly, with retry on failure) and caches upcoming events.
3. Events with a `timer:` tag in their description are eligible for auto-triggering.
4. At the event start time, the timer starts automatically with the configured duration and rounds.

### Timer tag format

Add a tag to the Hello Club event description:

```
timer: 12:3
```

This means 12-minute game duration, 3 rounds. If no tag is present, the system uses the configured defaults (12 minutes, 3 rounds).

To use continuous mode (rounds repeat until the booking ends):

```
timer: 12:0
```

Setting rounds to `0` enables continuous mode. The timer repeats rounds until the event end time (cutoff), then finishes.

### Mid-event boot recovery

If the ESP32 reboots during an active Hello Club event, it checks the cached events on startup, calculates which round should be in progress and how much time remains, and resumes the timer automatically. No manual intervention needed.

### Configuration

- **API key** -- Set via admin Settings panel
- **Default duration** -- Applied when no `timer:` tag is found (default: 12 minutes)
- **Default rounds** -- Applied when no `timer:` tag is found (default: 3)
- **Poll interval** -- 1 hour between API fetches; 5-minute retry on failure
- **Event cache** -- Up to 20 events cached in NVS; survives reboots

## Authentication

| Role | Access |
|------|--------|
| Viewer | View timer and schedules. No login required. |
| Operator | Start/stop timer, manage own club's schedules, change own password. |
| Admin | All operator capabilities plus user management, Hello Club config, QR code setup, timezone, factory reset. |

Default admin credentials: `admin` / `admin`. Change immediately after first login.

Passwords must be at least 5 characters. Sessions expire after 30 minutes of inactivity.

## QR Code Access

Admins can configure a WiFi QR code in Settings. When enabled, a QR icon appears on the timer page. Guests scan the code with their phone camera to join the WiFi network and are directed to the timer URL. Uses the `qrcode.min.js` library bundled in SPIFFS.

## Build Commands

```bash
pio run                                              # Build firmware
pio run --target upload                              # Upload via USB
pio run --target uploadfs                            # Upload web interface
pio run --target upload --upload-port badminton-timer.local  # OTA upload
pio device monitor                                   # Serial monitor (115200 baud)
```

## Project Structure

```
esp32-badminton-timer/
├── src/
│   ├── main.cpp              # Entry point, WiFi, WebSocket server, message routing
│   ├── timer.h/cpp           # Timer state machine (IDLE/RUNNING/PAUSED/FINISHED)
│   ├── siren.h/cpp           # Non-blocking relay control
│   ├── users.h/cpp           # Auth system, SHA-256 hashing, role management
│   ├── schedule.h/cpp        # Weekly recurring schedules
│   ├── helloclub.h/cpp       # Hello Club API client, event cache, boot recovery
│   ├── settings.h/cpp        # NVS persistence layer
│   ├── config.h              # All constants, limits, feature flags, pin assignments
│   └── wifi_credentials.h    # WiFi and OTA passwords (git-ignored)
├── data/
│   ├── index.html            # Web interface
│   ├── script.js             # WebSocket client, 60fps timer interpolation
│   ├── style.css             # Responsive CSS
│   ├── qrcode.min.js         # QR code generation library
│   └── qr-test.html          # QR code test page
├── test-server/
│   ├── server.js             # Node.js mock of entire WebSocket API
│   └── README.md             # Test server docs
├── tests/                    # Jest test suite
├── platformio.ini            # PlatformIO build configuration
├── API.md                    # WebSocket protocol reference
├── ARCHITECTURE.md           # System design documentation
├── INSTALL_GUIDE.md          # Hardware assembly and installation
├── USER_GUIDE.md             # End-user guide
└── CHANGELOG.md              # Version history
```

## Technology Stack

**ESP32 firmware (C++/Arduino):** AsyncWebServer, AsyncWebSocket, ArduinoJson, ezTime (NTP), Preferences (NVS), SPIFFS, HTTPClient (Hello Club API)

**Web frontend (vanilla JS):** WebSocket API, requestAnimationFrame (60fps countdown), CSS Grid (calendar), Web Audio API (sound effects)

**Development:** PlatformIO, Node.js/Express/ws (mock server), Jest (tests)

## Configuration

All tunable constants live in `src/config.h`:

| Setting | Default | Description |
|---------|---------|-------------|
| Game duration | 12 minutes | Per-round duration |
| Rounds | 3 | Rounds per match |
| Siren blast | 1000 ms | Siren on-time |
| Siren pause | 1000 ms | Gap between blasts |
| Session timeout | 30 minutes | Inactivity before auto-logout |
| Min password length | 5 characters | For all user accounts |
| Max operators | 10 | Operator account limit |
| Max schedules | 50 | Schedule entry limit |
| HC poll interval | 1 hour | Hello Club API fetch frequency |
| HC max cached events | 20 | Event cache size in NVS |

Feature flags: `ENABLE_WATCHDOG`, `ENABLE_OTA`, `ENABLE_MDNS`, `ENABLE_SELF_TEST`. Debug output controlled by `DEBUG_MODE`.

## Troubleshooting

**Timer page won't load** -- Try the IP address directly if `badminton-timer.local` does not resolve. Check router for the ESP32's DHCP lease.

**Schedules not auto-starting** -- Verify the NTP sync indicator shows a green checkmark. Confirm the scheduling toggle is ON. Check that the timezone is set correctly in Settings.

**Hello Club events not triggering** -- Confirm the API key is set. Check the event description contains a valid `timer:` tag. Review the serial monitor (115200 baud) for API errors. Events must be within the 2-minute trigger window of their start time.

**Session expired unexpectedly** -- Sessions time out after 30 minutes of inactivity. Any WebSocket message resets the timer.

## Documentation

| Document | Contents |
|----------|----------|
| [API.md](API.md) | WebSocket message protocol reference |
| [ARCHITECTURE.md](ARCHITECTURE.md) | System design and data flow |
| [INSTALL_GUIDE.md](INSTALL_GUIDE.md) | Hardware wiring and software installation |
| [USER_GUIDE.md](USER_GUIDE.md) | End-user guide for operators and admins |
| [CHANGELOG.md](CHANGELOG.md) | Detailed version history |
| [test-server/README.md](test-server/README.md) | Local development server |

## License

MIT License -- see LICENSE file for details.

---

Last updated: 2026-03-18
