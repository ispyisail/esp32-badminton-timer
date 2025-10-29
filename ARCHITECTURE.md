# ESP32 Badminton Timer - System Architecture

## Overview

The ESP32 Badminton Timer is a web-controlled countdown timer system designed for badminton courts. It uses a client-server architecture with real-time synchronization via WebSockets.

**Version**: 2.0.0
**Last Updated**: 2025-10-29

---

## System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      ESP32 Device                            │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Main Application (main.cpp)              │  │
│  │                                                        │  │
│  │  ┌──────────┐  ┌──────────┐  ┌───────────┐          │  │
│  │  │  Timer   │  │  Siren   │  │ Settings  │          │  │
│  │  │  Module  │  │  Module  │  │  Module   │          │  │
│  │  └──────────┘  └──────────┘  └───────────┘          │  │
│  │                                                        │  │
│  │  ┌────────────────────────────────────────────────┐  │  │
│  │  │        WebSocket Server (ws://.../ws)         │  │  │
│  │  └────────────────────────────────────────────────┘  │  │
│  │                                                        │  │
│  │  ┌────────────────────────────────────────────────┐  │  │
│  │  │         Web Server (HTTP Port 80)              │  │  │
│  │  │   Serves: index.html, script.js, style.css     │  │  │
│  │  └────────────────────────────────────────────────┘  │  │
│  │                                                        │  │
│  │  ┌────────────────────────────────────────────────┐  │  │
│  │  │          Hardware Interfaces                    │  │  │
│  │  │  • GPIO 26 (Relay/Siren)                       │  │  │
│  │  │  • WiFi Radio                                   │  │  │
│  │  │  • NVS (Preferences Storage)                   │  │  │
│  │  │  • SPIFFS (File System)                        │  │  │
│  │  └────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ WebSocket (bidirectional)
                              │ HTTP (static files)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Web Browser (Client)                       │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              User Interface (index.html)              │  │
│  │                                                        │  │
│  │  ┌────────────────────────────────────────────────┐  │  │
│  │  │  JavaScript Application (script.js)            │  │  │
│  │  │                                                 │  │  │
│  │  │  • WebSocket Client                            │  │  │
│  │  │  • requestAnimationFrame Timer Loop            │  │  │
│  │  │  • Authentication Manager                      │  │  │
│  │  │  • UI Controller                               │  │  │
│  │  │  • Keyboard Shortcut Handler                   │  │  │
│  │  │  • Sound Effects (Web Audio API)               │  │  │
│  │  └────────────────────────────────────────────────┘  │  │
│  │                                                        │  │
│  │  ┌────────────────────────────────────────────────┐  │  │
│  │  │           CSS Styling (style.css)              │  │  │
│  │  └────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## Component Breakdown

### Server-Side (ESP32)

#### 1. Core Modules

**Timer Module** (`timer.h/cpp`)
- Maintains authoritative timer state
- Handles game/break countdown logic
- Manages round transitions
- Provides overflow protection for `millis()`
- State machine: `IDLE → RUNNING → PAUSED/FINISHED → IDLE`

**Siren Module** (`siren.h/cpp`)
- Non-blocking relay control
- Configurable blast sequences
- State machine for multi-blast patterns
- No use of `delay()` - fully async

**Settings Module** (`settings.h/cpp`)
- Loads/saves configuration from NVS
- Handles preferences namespace
- Provides error handling for NVS failures

**Configuration** (`config.h`)
- Centralized constants
- Feature flags
- Debug macros
- Validation limits

#### 2. Main Application (`main.cpp`)

**Responsibilities:**
- System initialization (WiFi, SPIFFS, NVS, OTA)
- Watchdog timer setup and reset
- Self-test execution on boot
- WebSocket server management
- Authentication enforcement
- Message routing and validation
- HTTP server for static files
- Captive portal for WiFi setup
- mDNS service advertising

**Key Features:**
- Watchdog timer (30s timeout, auto-restart on hang)
- Self-test (SPIFFS, NVS, Relay verification)
- Debug mode with configurable logging
- WebSocket authentication with per-client tracking
- Server-side input validation
- Periodic timer sync broadcasts (every 5s)
- Match completion fanfare (3 blasts)

#### 3. WiFi Management

**Two-Stage Connection:**
1. **Primary**: Try hardcoded credentials from `wifi_credentials.h`
2. **Fallback**: Captive portal (`BadmintonTimerSetup`)

**Captive Portal Detection:**
- Android: `/generate_204`, `/gen_204`
- iOS/macOS: `/hotspot-detect.html`, `/library/test/success.html`
- Windows: `/connecttest.txt`, `/ncsi.txt`

**Timeout**: 5 minutes, 5 retry attempts

#### 4. Security

**WebSocket Authentication:**
- Password required (`WEB_PASSWORD` from credentials file)
- Per-client auth state tracking
- Auth required for all control actions
- Session-based (re-auth after disconnect)

**OTA Protection:**
- Password required (`OTA_PASSWORD`)
- Hostname validation

---

### Client-Side (Web Browser)

#### 1. Timer Synchronization Architecture

**Problem Solved:** Eliminate jitter, drift, and backwards counting

**Solution: Hybrid Client-Server Sync**

```javascript
// Server broadcasts every 5 seconds:
{
    "event": "sync",
    "mainTimerRemaining": 1234567,  // Actual time left (ms)
    "breakTimerRemaining": 45678,
    "serverMillis": 98765432,       // Server timestamp
    "status": "RUNNING"
}

// Client calculates current time:
function clientTimerLoop() {
    const localElapsed = Date.now() - lastSyncTime;
    const currentTimer = serverTimerRemaining - localElapsed;
    // Update display at 60fps using requestAnimationFrame
}
```

**Benefits:**
- **Smooth**: 60fps updates via `requestAnimationFrame`
- **Accurate**: Resync every 5s prevents drift
- **Resilient**: Continues counting between syncs
- **No jitter**: No abrupt value replacements

#### 2. WebSocket Message Flow

```
Client                          Server
  │                               │
  ├─── Connect ───────────────────▶
  │                               │
  ◀─── Auth Request ──────────────┤
  │                               │
  ├─── Authenticate ──────────────▶
  │                               │
  ◀─── Auth Success ──────────────┤
  ◀─── Settings ──────────────────┤
  ◀─── State ─────────────────────┤
  │                               │
  ├─── Action: start ─────────────▶
  │                               │
  ◀─── Event: start ──────────────┤ (broadcast to all)
  ◀─── Sync ──────────────────────┤ (every 5s)
  ◀─── Sync ──────────────────────┤
  ◀─── Sync ──────────────────────┤
  │                               │
  ├─── Action: pause ─────────────▶
  │                               │
  ◀─── Event: pause ──────────────┤
  │                               │
```

#### 3. User Interface Features

**Keyboard Shortcuts:**
- `SPACE`: Pause/Resume
- `R`: Reset
- `S`: Start
- `M`: Toggle sound effects
- `?`: Show help

**Visual Feedback:**
- Connection status indicator (green/red dot)
- Button disabled states during processing
- Loading overlay on connect
- Temporary notification banners
- Smooth animations (fade in/out)

**Sound Effects** (optional, toggle with `M`):
- Start: 880 Hz beep
- Pause/Resume: 660 Hz beep
- Reset: 440 Hz beep
- Implemented with Web Audio API

#### 4. Reconnection Strategy

**Exponential Backoff:**
```
Attempt 1: 2s delay
Attempt 2: 4s delay
Attempt 3: 8s delay
Attempt 4: 16s delay
Attempt 5: 30s delay (capped)
...
Attempt 10: Give up, show error
```

**Auto-reconnect on:**
- Network disconnect
- Server restart
- WebSocket timeout

---

## Data Flow

### 1. Timer Start Sequence

```
User clicks "Start"
    ↓
Client: Disable buttons, play sound, send { action: "start" }
    ↓
Server: Validate state, authenticate client
    ↓
Server: timer.start() → RUNNING state
    ↓
Server: Broadcast { event: "start", gameDuration, breakDuration, ... }
    ↓
All Clients: Receive start event
    ↓
All Clients: Start requestAnimationFrame loop
    ↓
Display updates at 60fps
    ↓
Server: Periodic sync broadcast every 5s
    ↓
Clients: Update sync timestamp, correct drift
```

### 2. Settings Change Sequence

```
User changes setting in UI
    ↓
Client: Send { action: "save_settings", settings: {...} }
    ↓
Server: Validate input (range checks)
    ↓
Server: If invalid → send error, return
    ↓
Server: If valid → save to NVS
    ↓
Server: Broadcast { event: "settings", settings: {...} }
    ↓
All Clients: Update UI with new settings
    ↓
Client: Show "Settings saved" notification
```

### 3. Round Completion Sequence

```
Server: mainTimerRemaining == 0
    ↓
Server: startSiren(2) → 2 blasts for round end
    ↓
Server: currentRound < numRounds?
    ├─ YES: currentRound++, restart timers
    │   ↓
    │   Server: Broadcast { event: "new_round", ... }
    │   ↓
    │   Clients: Reset to new round duration
    │
    └─ NO: Match finished
        ↓
        Server: timerState = FINISHED
        ↓
        Server: startSiren(3) → 3 blasts for match completion
        ↓
        Server: Broadcast { event: "finished" }
        ↓
        Clients: Stop timer, reset display
```

---

## State Machines

### Timer State Machine

```
┌────────┐
│  IDLE  │◀─────────────────────┐
└────────┘                      │
    │                           │
    │ start()                   │ reset()
    ▼                           │
┌────────┐     pause()      ┌────────┐
│RUNNING │─────────────────▶│ PAUSED │
└────────┘                   └────────┘
    │ ▲                         │
    │ │ resume()                │ reset()
    │ └─────────────────────────┤
    │                           │
    │ round complete            │
    │ && currentRound >=        │
    │    numRounds              │
    ▼                           │
┌──────────┐                   │
│ FINISHED │───────────────────┘
└──────────┘     reset()
```

### WebSocket Connection State Machine

```
┌──────────────┐
│ DISCONNECTED │
└──────────────┘
        │
        │ connect()
        ▼
┌──────────────┐
│  CONNECTING  │──────┐
└──────────────┘      │
        │             │ onclose()
        │ onopen()    │
        ▼             ▼
┌──────────────┐  ┌──────────────┐
│  CONNECTED   │  │ RECONNECTING │
│(need auth)   │  │ (exp backoff)│
└──────────────┘  └──────────────┘
        │             ▲     │
        │ auth_success│     │ max attempts
        ▼             │     ▼
┌──────────────┐     │  ┌──────────────┐
│AUTHENTICATED │─────┘  │    FAILED    │
└──────────────┘        └──────────────┘
```

---

## Timing & Synchronization

### Server-Side Timing

**Authority**: ESP32 is the single source of truth

**Method**: `millis()` based elapsed time calculation
```cpp
unsigned long now = millis();
unsigned long elapsed = calculateElapsed(mainTimerStart, now);
mainTimerRemaining = (elapsed < gameDuration) ? (gameDuration - elapsed) : 0;
```

**Overflow Protection**: Handles `millis()` wraparound after ~49 days
```cpp
unsigned long calculateElapsed(unsigned long start, unsigned long now) {
    if (now >= start) {
        return now - start;
    } else {
        // Overflow occurred
        return (0xFFFFFFFF - start) + now + 1;
    }
}
```

**Sync Broadcast**: Every 5 seconds during RUNNING state

### Client-Side Timing

**Method**: Calculate from last sync + local elapsed
```javascript
const localElapsed = Date.now() - lastSyncTime;
const currentTimer = serverTimerRemaining - localElapsed;
```

**Display Update**: `requestAnimationFrame` (60fps)

**Precision**: Sub-second accuracy, no accumulated drift

---

## Persistence

**Storage**: ESP32 NVS (Non-Volatile Storage)

**Namespace**: "timer"

**Keys Stored:**
- `gameDuration` (unsigned long, milliseconds)
- `breakDuration` (unsigned long, milliseconds)
- `numRounds` (unsigned int)
- `breakEnabled` (bool)
- `sirenLength` (unsigned long, milliseconds)
- `sirenPause` (unsigned long, milliseconds)

**Load**: On boot (with defaults if NVS unavailable)

**Save**: When user changes settings via web interface

---

## Error Handling

### Server-Side

- **SPIFFS failure**: Restart ESP32 after 5s
- **NVS failure**: Use defaults, log error
- **WiFi failure**: Fall back to captive portal
- **JSON overflow**: Send error to client
- **Invalid input**: Validate and reject with error message
- **Watchdog timeout**: Auto-restart ESP32

### Client-Side

- **WebSocket disconnect**: Exponential backoff reconnect
- **Authentication failure**: Prompt for password again
- **Max reconnect attempts**: Show error, suggest page refresh
- **Invalid settings**: Show error banner

---

## Security

### Threats Mitigated

| Threat | Mitigation |
|--------|------------|
| Unauthorized timer control | WebSocket password authentication |
| Unauthorized firmware updates | OTA password protection |
| Input validation attacks | Server-side range checking |
| Replay attacks | Session-based authentication |
| Network eavesdropping | Use on trusted local network only |

### Recommendations

- Change default passwords in `wifi_credentials.h`
- Use strong, unique passwords
- Keep WiFi network secured (WPA2/WPA3)
- Only expose timer on trusted networks
- Regularly update firmware

---

## Performance Characteristics

| Metric | Value |
|--------|-------|
| WebSocket latency | 5-50ms (local network) |
| Timer precision | ±10ms (server-side) |
| Display update rate | 60fps (client-side) |
| Sync interval | 5 seconds |
| Memory usage (ESP32) | ~40KB RAM |
| Flash usage (ESP32) | ~1MB program + ~200KB SPIFFS |
| Max WebSocket clients | 10 (configurable) |
| Watchdog timeout | 30 seconds |

---

## Debugging

### Enable Debug Mode

In `config.h`:
```cpp
#define DEBUG_MODE 1  // Enable
#define DEBUG_MODE 0  // Disable
```

**Output**: Serial console at 115200 baud

**Debug Information:**
- Firmware version and build time
- Self-test results
- WiFi connection attempts
- WebSocket connect/disconnect events
- Authentication attempts
- Timer state changes
- Settings load/save operations
- Siren activation sequences

### Common Issues

**Timer not syncing:**
- Check WebSocket connection status (green dot)
- Check serial output for sync broadcasts
- Verify client receives sync messages (browser console)

**Android WiFi portal issues:**
- Disable "Smart Network Switch" in Android settings
- Tap "Stay connected" when prompted
- Try forgetting network and reconnecting

**Watchdog resets:**
- Check for blocking code in main loop
- Verify no long delays
- Review serial output before reset

---

## Future Enhancements

Potential improvements for v3.0:
- HTTPS support with self-signed certificates
- Multi-court support (multiple timers)
- Statistics tracking (matches played, total time)
- Mobile app (React Native / Flutter)
- Cloud backup of settings
- RESTful API in addition to WebSocket
- LCD display support for local viewing
- BLE control for offline operation

---

## References

- **ESP32 Documentation**: https://docs.espressif.com/projects/esp-idf/
- **ArduinoJson**: https://arduinojson.org/
- **ESPAsyncWebServer**: https://github.com/me-no-dev/ESPAsyncWebServer
- **Web Audio API**: https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API
- **requestAnimationFrame**: https://developer.mozilla.org/en-US/docs/Web/API/window/requestAnimationFrame

---

**Last Updated**: 2025-10-29
**Document Version**: 2.0
**Author**: Claude Code
**Project**: ESP32 Badminton Timer
