# ESP32 Badminton Timer - System Architecture

## Overview

The ESP32 Badminton Timer is a web-controlled timer system designed for badminton courts with multi-club scheduling, Hello Club booking integration, and role-based authentication. It uses a client-server architecture with real-time synchronization via WebSockets.

**Version**: 3.1.0
**Last Updated**: 2026-03-18

---

## System Architecture Diagram

```
+-------------------------------------------------------------+
|                      ESP32 Device                            |
|                                                              |
|  +--------------------------------------------------------+  |
|  |              Main Application (main.cpp)                |  |
|  |                                                         |  |
|  |  +----------+  +----------+  +-----------+             |  |
|  |  |  Timer   |  |  Siren   |  | Settings  |             |  |
|  |  |  Module  |  |  Module  |  |  Module   |             |  |
|  |  +----------+  +----------+  +-----------+             |  |
|  |                                                         |  |
|  |  +----------+  +----------+  +-------------+           |  |
|  |  |   User   |  | Schedule |  | Hello Club  |           |  |
|  |  | Manager  |  | Manager  |  |   Client    |           |  |
|  |  +----------+  +----------+  +-------------+           |  |
|  |                                  |                      |  |
|  |                                  | HTTPS (fetch events) |  |
|  |                                  v                      |  |
|  |                          api.helloclub.com              |  |
|  |                                                         |  |
|  |  +--------------------------------------------------+  |  |
|  |  |        WebSocket Server (ws://.../ws)             |  |  |
|  |  |  * Authentication                                 |  |  |
|  |  |  * Rate Limiting (10 msg/sec)                     |  |  |
|  |  |  * Session Timeout (30 min)                       |  |  |
|  |  +--------------------------------------------------+  |  |
|  |                                                         |  |
|  |  +--------------------------------------------------+  |  |
|  |  |         Web Server (HTTP Port 80)                 |  |  |
|  |  |   Serves: index.html, script.js, style.css,      |  |  |
|  |  |           qrcode.min.js                           |  |  |
|  |  +--------------------------------------------------+  |  |
|  |                                                         |  |
|  |  +--------------------------------------------------+  |  |
|  |  |          Hardware Interfaces                      |  |  |
|  |  |  * GPIO 26 (Relay/Siren)                          |  |  |
|  |  |  * WiFi Radio                                     |  |  |
|  |  |  * NVS (Preferences Storage)                      |  |  |
|  |  |  * SPIFFS (File System + Boot Log)                |  |  |
|  |  |  * ezTime (NTP Client)                            |  |  |
|  |  +--------------------------------------------------+  |  |
|  +--------------------------------------------------------+  |
+-------------------------------------------------------------+
                              |
                              | WebSocket (bidirectional)
                              | HTTP (static files)
                              v
+-------------------------------------------------------------+
|                   Web Browser (Client)                       |
|                                                              |
|  +--------------------------------------------------------+  |
|  |              User Interface (index.html)                |  |
|  |                                                         |  |
|  |  +--------------------------------------------------+  |  |
|  |  |  JavaScript Application (script.js)               |  |  |
|  |  |                                                    |  |  |
|  |  |  * WebSocket Client                               |  |  |
|  |  |  * requestAnimationFrame Timer Loop               |  |  |
|  |  |  * Authentication Manager                         |  |  |
|  |  |  * Schedule Manager                               |  |  |
|  |  |  * Hello Club Events Display                      |  |  |
|  |  |  * QR Code Generator                              |  |  |
|  |  |  * Calendar Renderer (CSS Grid)                   |  |  |
|  |  |  * UI Controller                                  |  |  |
|  |  |  * Keyboard Shortcut Handler                      |  |  |
|  |  |  * Sound Effects (Web Audio API)                  |  |  |
|  |  +--------------------------------------------------+  |  |
|  |                                                         |  |
|  |  +--------------------------------------------------+  |  |
|  |  |           CSS Styling (style.css)                 |  |  |
|  |  |  * Login Modal                                    |  |  |
|  |  |  * User Management Page                           |  |  |
|  |  |  * Schedule Page with Weekly Calendar             |  |  |
|  |  |  * Hello Club Settings Panel                      |  |  |
|  |  |  * QR Code Configuration Panel                    |  |  |
|  |  |  * Role-Based Visibility (CSS classes)            |  |  |
|  |  +--------------------------------------------------+  |  |
|  +--------------------------------------------------------+  |
+-------------------------------------------------------------+
```

---

## Component Breakdown

### Server-Side (ESP32)

#### 1. Core Modules

**User Manager** (`users.h/cpp`) - NEW in v3.0, updated in v3.1
- Three-tier role-based authentication (VIEWER, OPERATOR, ADMIN)
- SHA-256 password hashing with automatic plaintext migration (v3.1)
- NVS persistence for operators
- Default admin account (admin/admin)
- Password validation (minimum 5 characters)
- Factory reset capability
- State: Admin password hash (runtime), Operators list (NVS)

**Schedule Manager** (`schedule.h/cpp`) - NEW in v3.0
- Weekly recurring schedule management
- Club-based organization with owner tracking
- Week-minute calculation (0-10079 minutes from Sunday 00:00)
- Auto-trigger detection with 2-minute cooldown
- Permission checking (operators see only their schedules)
- Up to 50 schedules stored in NVS
- Unique ID generation with collision detection
- State machine: `ENABLED/DISABLED` global toggle

**Hello Club Client** (`helloclub.h/cpp`) - NEW in v3.1
- Integration with the Hello Club external booking system API
- Non-blocking HTTPS fetch with retry logic and request timeout (10s)
- HTTPS certificate validation (Google Trust Services Root R4 + Let's Encrypt ISRG Root X1)
- Event caching in NVS (up to 20 events)
- Timer tag parsing from event descriptions (format: `timer: duration:rounds`)
- Auto-trigger when event start time matches current time (2-minute window)
- Mid-event boot recovery: if the device reboots during an active event, it detects this on boot and resumes the timer at the correct round and remaining time using `startMidRound()`
- Event cutoff enforcement: hard stop when event end time is reached
- Cancel flag persistence in NVS: if an operator manually resets during an event, the cancel flag prevents boot recovery from restarting it
- Hourly polling with retry on failure (5-minute retry interval)
- Expired event purging
- State: API key, enabled flag, cached events, cancel flag (all in NVS)

**Timer Module** (`timer.h/cpp`) - updated in v3.1
- Maintains authoritative timer state
- Handles game/break countdown logic
- Manages round transitions
- Provides overflow protection for `millis()`
- Pause After Next: one-shot flag that pauses the timer between rounds (v3.1)
- Continuous Mode: rounds repeat indefinitely until external stop, used by Hello Club events with 0 rounds (v3.1)
- `startMidRound(round, remainingMs)`: resume timer at a specific round and remaining time, used for mid-event boot recovery (v3.1)
- State machine: `IDLE -> RUNNING -> PAUSED/FINISHED -> IDLE`

**Siren Module** (`siren.h/cpp`)
- Non-blocking relay control
- Configurable blast sequences
- State machine for multi-blast patterns
- No use of `delay()` - fully async

**Settings Module** (`settings.h/cpp`) - updated in v3.1
- Loads/saves configuration from NVS
- Handles preferences namespace
- Provides error handling for NVS failures
- Hello Club default round duration setting (v3.1)
- QR code settings: guest WiFi SSID override, password, encryption type (v3.1)

**Configuration** (`config.h`)
- Centralized constants
- Feature flags
- Debug macros
- Validation limits
- Hello Club API configuration (poll interval, retry interval, max cached events, trigger window)

#### 2. Main Application (`main.cpp`)

**Responsibilities:**
- System initialization (WiFi, SPIFFS, NVS, OTA)
- Boot logger initialization (persists to SPIFFS for debugging reset reasons)
- Watchdog timer setup and reset
- Self-test execution on boot
- Mid-event boot recovery check and timer resumption (v3.1)
- WebSocket server management
- Authentication enforcement (v3.0)
- Rate limiting (10 messages/second per client) (v3.0)
- Session timeout (30 minutes inactivity) (v3.0)
- Message routing and validation
- HTTP server for static files
- Captive portal for WiFi setup
- mDNS service advertising
- NTP status monitoring and broadcasting (v3.0)
- Schedule auto-triggering (v3.0)
- Hello Club event polling and auto-triggering (v3.1)
- Hello Club event cutoff enforcement (v3.1)

**Key Features:**
- Watchdog timer (30s timeout, auto-restart on hang)
- Self-test (SPIFFS, NVS, Relay verification)
- Boot logger: writes timestamped entries to `/bootlog.txt` on SPIFFS, including firmware version, reset reason, free heap, and WiFi connection details; auto-truncates at 8KB (v3.1)
- Debug mode with configurable logging
- Per-client authentication tracking with username storage (v3.0)
- Per-client rate limiting with sliding window (v3.0)
- Session timeout with automatic viewer downgrade (v3.0)
- Server-side input validation with XSS protection (HTML escaping) (v3.1)
- Periodic timer sync broadcasts (every 5s)
- NTP sync status checks (every 5s) (v3.0)
- Schedule trigger checks (every 30s) (v3.0)
- Hello Club event trigger checks (every 30s, when enabled) (v3.1)
- Match completion fanfare (3 blasts)

#### 3. Client Tracking (v3.0)

**Authentication State:**
```cpp
std::map<uint32_t, UserRole> authenticatedClients;
std::map<uint32_t, String> authenticatedUsernames;
```

**Rate Limiting State:**
```cpp
struct RateLimitInfo {
    unsigned long windowStart;
    int messageCount;
};
std::map<uint32_t, RateLimitInfo> clientRateLimits;
const int MAX_MESSAGES_PER_SECOND = 10;
const unsigned long RATE_LIMIT_WINDOW = 1000;
```

**Session Management:**
```cpp
std::map<uint32_t, unsigned long> clientLastActivity;
const unsigned long SESSION_TIMEOUT = 30 * 60 * 1000; // 30 minutes
```

**Cleanup on Disconnect:**
- Remove from authenticatedClients
- Remove from authenticatedUsernames
- Remove from clientRateLimits
- Remove from clientLastActivity

#### 4. WiFi Management

**Two-Stage Connection:**
1. **Primary**: Try hardcoded credentials from `wifi_credentials.h`
2. **Fallback**: Captive portal (`BadmintonTimerSetup`)

**Captive Portal Detection:**
- Android: `/generate_204`, `/gen_204`
- iOS/macOS: `/hotspot-detect.html`, `/library/test/success.html`
- Windows: `/connecttest.txt`, `/ncsi.txt`

**Timeout**: 5 minutes, 5 retry attempts

#### 5. Security (Enhanced in v3.0, v3.1)

**WebSocket Authentication:**
- Role-based access control (VIEWER, OPERATOR, ADMIN)
- Per-client authentication state
- Username tracking for schedule ownership (v3.0)
- Session timeout after 30 minutes inactivity (v3.0)
- Rate limiting (10 messages/second per client) (v3.0)
- Server-side ownership enforcement - client cannot spoof ownerUsername (v3.0)

**Password Security (v3.1):**
- SHA-256 password hashing
- Automatic plaintext-to-hash migration on first login after upgrade
- Minimum password length: 5 characters

**HTTPS Certificate Validation (v3.1):**
- Hello Club API requests use WiFiClientSecure with pinned root CAs
- Google Trust Services Root R4 (valid until 2036)
- Let's Encrypt ISRG Root X1 (valid until 2035)

**XSS Protection (v3.1):**
- HTML escaping applied to user-supplied strings before rendering

**Permission Hierarchy:**
- VIEWER: View only
- OPERATOR: View + Control timer + Manage own schedules
- ADMIN: Everything + User management + View all schedules + save_settings + save_helloclub_settings + save_qr_settings

**OTA Protection:**
- Password required (`OTA_PASSWORD`)
- Hostname validation

**Input Validation:**
- All schedule fields validated (day 0-6, hour 0-23, minute 0-59)
- All settings ranges checked
- Minimum password length (5 characters)
- Error codes with ERR_ prefix

---

### Client-Side (Web Browser)

#### 1. Authentication Flow (NEW in v3.0)

**Login Process:**
```
User opens page
    |
Login modal displayed
    |
User chooses:
+-- Enter credentials (admin/operator)
|   |
|   Send authenticate action with username/password
|   |
|   Server validates against UserManager (SHA-256 hash comparison)
|   |
|   Success: Receive auth_success, set userRole, hide modal
|   Failure: Show error message
|
+-- Click "Continue as Viewer"
    |
    Send authenticate with empty credentials
    |
    Server grants VIEWER role
    |
    Receive viewer_mode event, controls disabled
```

**Role-Based UI:**
```css
body.role-viewer .controls button {
    opacity: 0.5;
    pointer-events: none;
}
body.role-operator .admin-only {
    display: none;
}
```

**Session Timeout:**
- Server tracks last activity per client
- After 30 minutes: Downgrade to VIEWER, send session_timeout event
- Client shows notification, disables controls

#### 2. Schedule Management (NEW in v3.0)

**Calendar Rendering:**
- CSS Grid layout: 8 columns (time + 7 days)
- Rows for each hour (6 AM - 11 PM)
- Dynamic schedule blocks positioned in cells
- Click handler on each block for edit/delete

**Schedule CRUD:**
```javascript
// Create
saveSchedule() -> send add_schedule with nested { schedule: {...} }
receive schedule_added -> add to local array, re-render calendar

// Read
on page load -> send get_schedules
receive schedules_list -> store in local array, render calendar

// Update
editSchedule(id) -> populate form
saveSchedule() -> send update_schedule with nested { schedule: {...} }
receive schedule_updated -> update local array, re-render

// Delete
deleteSchedule(id) -> send delete_schedule
receive schedule_deleted -> remove from local array, re-render
```

**Permission Filtering:**
- Operator: Only see schedules where ownerUsername matches their username
- Admin: See all schedules from all clubs

#### 3. Timer Synchronization Architecture

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

#### 4. WebSocket Message Flow

```
Client                          Server
  |                               |
  +--- Connect ------------------>
  |                               |
  <--- State ---------------------+
  |                               |
  +--- Action: authenticate ----->  (username + password)
  |                               |
  <--- Event: auth_success -------+
  <--- Event: schedules_list -----+
  <--- Event: helloclub_events ---+  (if HC enabled)
  |                               |
  +--- Action: start ------------>  (authenticated)
  |                               |
  <--- Event: start --------------+  (broadcast to all)
  <--- Event: sync ---------------+  (every 5s)
  <--- Event: sync ---------------+
  |                               |
  +--- Action: add_schedule ----->  (operator/admin only)
  |                               |
  <--- Event: schedule_added -----+
  |                               |
  <--- Event: event_cutoff -------+  (HC event end time reached)
  |                               |
  <--- Event: session_timeout ----+  (after 30 min inactivity)
  |                               |
```

#### 5. User Interface Features

**Keyboard Shortcuts:**
- `SPACE`: Pause/Resume
- `R`: Reset
- `S`: Start
- `M`: Toggle sound effects
- `?`: Show help

**Visual Feedback:**
- Connection status indicator (green/red dot)
- NTP sync status indicator (v3.0)
- Role badge (Viewer/Operator/Admin) (v3.0)
- Button disabled states during processing
- Loading overlay on connect
- Temporary notification banners
- Smooth animations (fade in/out)

**Sound Effects** (optional, toggle with `M`):
- Start: 880 Hz beep
- Pause/Resume: 660 Hz beep
- Reset: 440 Hz beep
- Implemented with Web Audio API

**Pages and Panels:**
- **Login Modal**: Credentials input with "Continue as Viewer" option (v3.0)
- **User Management Page**: Add/remove operators (admin only) (v3.0)
- **Schedule Page**: Weekly calendar grid, add/edit/delete schedules (v3.0)
- **Hello Club Settings Panel**: API key, enable/disable, default duration (admin only) (v3.1)
- **QR Code Configuration Panel**: Guest WiFi SSID override, password, encryption type; generates QR codes for easy guest access (admin only) (v3.1)

#### 6. Reconnection Strategy

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

### 1. Authentication Sequence (NEW in v3.0)

```
User clicks Login
    |
Client: Disable buttons, send { action: "authenticate", username, password }
    |
Server: Validate credentials via UserManager (SHA-256 hash comparison)
    |
Server: If admin/admin -> role = ADMIN
        If operator found -> role = OPERATOR
        If empty credentials -> role = VIEWER
        Else -> send error
    |
Server: Store authenticatedClients[clientId] = role
        Store authenticatedUsernames[clientId] = username
    |
Server: Send { event: "auth_success", role, username }
    |
Client: Set userRole, currentUsername
        Hide login modal
        Update UI for role (hide/show elements)
        Enable controls (if not viewer)
```

### 2. Schedule Creation Sequence (NEW in v3.0)

```
Operator fills schedule form
    |
Client: Send { action: "add_schedule", schedule: {clubName, dayOfWeek, ...} }
    |
Server: Check authentication (reject if VIEWER)
    |
Server: Generate unique schedule ID (timestamp + counter)
    |
Server: Set ownerUsername = authenticatedUsernames[clientId] (NOT from client!)
    |
Server: Validate all fields (day 0-6, hour 0-23, etc.)
    |
Server: If invalid -> send error, return
    |
Server: scheduleManager.addSchedule()
    |
Server: Save to NVS
    |
Server: Send { event: "schedule_added", schedule: {...} } to requester
    |
Client: Add schedule to local array
        Re-render calendar
        Show "Schedule added" notification
```

### 3. Schedule Auto-Trigger Sequence (NEW in v3.0)

```
Server: Every 30 seconds in loop()
    |
Server: Check scheduleManager.checkScheduleTrigger()
    |
Server: Calculate current week-minute (0-10079)
        For each enabled schedule:
            Calculate schedule's week-minute
            If match && not triggered in last 2 minutes:
                |
                Server: Start timer with schedule duration
                |
                Server: Broadcast { event: "start", ... }
                |
                All Clients: Timer starts automatically
                |
                Server: Record lastTriggerTime for this schedule
```

### 4. Hello Club Auto-Trigger Sequence (NEW in v3.1)

```
Server: Hourly, fetch events from api.helloclub.com via HTTPS
    |
Server: Filter events with "timer:" tag in description
    |
Server: Parse tag format "timer: duration:rounds" (e.g. "timer: 12:3")
    |
Server: Cache events in memory and NVS (up to 20)
    |
Server: Every 30 seconds in loop(), check cached events:
        For each untriggered event:
            If current time is within 2-minute window of event start:
                |
                Server: Mark event as triggered
                |
                Server: Set timer duration and rounds from parsed tag
                |
                Server: If rounds == 0, enable continuous mode
                |
                Server: timer.start()
                |
                Server: Track active event ID and end time
                |
                Server: Broadcast start event to all clients
                |
                All Clients: Timer starts, event name displayed
    |
Server: During RUNNING state, check if current time >= event end time:
        If cutoff reached:
            |
            Server: timer.reset()
            |
            Server: Broadcast reset/cutoff event
            |
            All Clients: Timer stops, cutoff notification shown
```

### 5. Mid-Event Boot Recovery Sequence (NEW in v3.1)

```
Device reboots (power cycle, watchdog, crash)
    |
Server: boot setup() runs
    |
Server: Load Hello Club cached events from NVS
    |
Server: Load cancel flag from NVS (helloclub namespace, "evt_cancel" key)
    |
Server: Call helloClubClient.checkMidEventRecovery()
    |
Server: For each triggered event where now is between startTime and endTime:
        Calculate elapsed time since event start
        Determine current round and remaining time in that round
        Return RecoveryResult { shouldRecover, currentRound, remainingMs, ... }
    |
Server: If shouldRecover AND event ID != cancel flag:
        |
        Server: timer.setGameDuration(durationMin * 60000)
        Server: timer.setNumRounds(numRounds)
        Server: If numRounds == 0, enable continuous mode
        Server: timer.startMidRound(currentRound, remainingMs)
        Server: Track active event ID and end time
        Server: Broadcast state update to connected clients
        Server: Log recovery to boot log
    |
Server: If shouldRecover AND event ID == cancel flag:
        |
        Server: Skip recovery (operator intentionally cancelled before reboot)
        Server: Log skip to boot log
```

### 6. Event Cutoff Sequence (NEW in v3.1)

```
Server: Timer is RUNNING with an active Hello Club event
    |
Server: Every loop() iteration, check if current time >= event end time
    |
Server: If cutoff time reached:
        |
        Server: timer.reset()
        Server: Clear active event tracking
        Server: Broadcast { event: "reset" } to all clients
        Server: Log cutoff
    |
All Clients: Timer resets, UI returns to idle state
```

### 7. Session Timeout Sequence (NEW in v3.0)

```
Server: Every 60 seconds in loop()
    |
Server: For each connected client:
        Check time since clientLastActivity[clientId]
    |
Server: If > 30 minutes && role != VIEWER:
        |
        Server: Set authenticatedClients[clientId] = VIEWER
                Remove authenticatedUsernames[clientId]
        |
        Server: Send { event: "session_timeout" } to client
    |
Client: Set userRole = 'viewer'
        Update UI (disable controls)
        Show notification: "Session expired - Continue as viewer or login again"
```

### 8. NTP Status Update Sequence (NEW in v3.0)

```
Server: Every 5 seconds in loop()
    |
Server: Check if ezTime sync status changed
    |
Server: Validate sync (year 2020-2100, month 1-12, day 1-31)
    |
Server: If status changed:
        |
        Server: Broadcast { event: "ntp_status", synced, time, ... }
    |
All Clients: Update NTP indicator
```

### 9. Timer Start Sequence (Updated for v3.0)

```
User clicks "Start"
    |
Client: Disable buttons, play sound, send { action: "start" }
    |
Server: Check authentication (reject if VIEWER or not authenticated)
    |
Server: Validate state, authenticate client
    |
Server: timer.start() -> RUNNING state
    |
Server: Broadcast { event: "start", gameDuration, breakDuration, ... }
    |
All Clients: Receive start event
    |
All Clients: Start requestAnimationFrame loop
    |
Display updates at 60fps
    |
Server: Periodic sync broadcast every 5s
    |
Clients: Update sync timestamp, correct drift
```

---

## State Machines

### Authentication State Machine (NEW in v3.0)

```
+--------------+
|  NO_AUTH     |
| (connected)  |
+--------------+
        |
        | authenticate (empty credentials)
        +--------------------+
        |                    v
        |              +----------+
        |              |  VIEWER  |
        |              +----------+
        |
        | authenticate (credentials)
        +---------------------+
        |                     |
        v                     v
+--------------+      +--------------+
|   OPERATOR   |      |    ADMIN     |
+--------------+      +--------------+
        |                     |
        | session_timeout     | session_timeout
        +---------+-----------+
                  |
                  v
            +----------+
            |  VIEWER  |
            +----------+
```

### Schedule System State Machine (NEW in v3.0)

```
+----------------+
| SCHEDULING OFF |
+----------------+
        |
        | set_scheduling(enabled: true)
        v
+----------------+
| SCHEDULING ON  |<-----+
+----------------+      |
        |               |
        | Every 30s     |
        v               |
+----------------+      |
| CHECK TRIGGERS |      |
+----------------+      |
        |               |
        | Match found?  |
        +-- NO ---------+
        |
        +-- YES
           |
    +----------------+
    | START TIMER    |
    | (auto-trigger) |
    +----------------+
```

### Timer State Machine (Updated for v3.1)

```
+--------+
|  IDLE  |<-------------------------+
+--------+                          |
    |                               |
    | start() or                    | reset()
    | startMidRound(round, ms)      |
    v                               |
+--------+     pause()          +--------+
|RUNNING |--------------------->| PAUSED |
+--------+                      +--------+
    | ^                             |
    | | resume()                    | reset()
    | +-----------------------------+
    |                               |
    | round complete                |
    | && currentRound >=            |
    |    numRounds                  |
    | (skipped in continuous mode)  |
    v                               |
+----------+                        |
| FINISHED |------------------------+
+----------+     reset()

Pause After Next (one-shot):
    RUNNING -> round ends -> if pauseAfterNext set:
        enter PAUSED instead of starting next round
        clear pauseAfterNext flag

Continuous Mode:
    RUNNING -> round ends -> always start next round
    (never enters FINISHED until external reset or event cutoff)
```

### WebSocket Connection State Machine (Updated for v3.0)

```
+--------------+
| DISCONNECTED |
+--------------+
        |
        | connect()
        v
+--------------+
|  CONNECTING  |------+
+--------------+      |
        |             | onclose()
        | onopen()    |
        v             v
+--------------+  +--------------+
|  CONNECTED   |  | RECONNECTING |
| (no auth)    |  | (exp backoff)|
+--------------+  +--------------+
        |             ^     |
        | authenticate|     | max attempts
        v             |     v
+--------------+      |  +--------------+
|AUTHENTICATED |------+  |    FAILED    |
|(viewer/op/ad)|         +--------------+
+--------------+
        |
        | session_timeout (auto-downgrade)
        v
+--------------+
|AUTHENTICATED |
|  (viewer)    |
+--------------+
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

### NTP Synchronization (NEW in v3.0)

**Library**: ezTime

**Configuration:**
- Timezone: Pacific/Auckland
- Auto-sync interval: 30 minutes
- NTP pool: pool.ntp.org

**Validation:**
```cpp
bool isTimeValid() {
    return (myTZ.year() > 2020 && myTZ.year() < 2100 &&
            myTZ.month() >= 1 && myTZ.month() <= 12 &&
            myTZ.day() >= 1 && myTZ.day() <= 31);
}
```

**Status Broadcast**: Every 5 seconds, only on status change

**Used For:**
- Schedule auto-triggering
- Hello Club event auto-triggering and cutoff enforcement
- Mid-event boot recovery (elapsed time calculation)
- Display current time
- Timestamp creation for schedules

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

**Storage**: ESP32 NVS (Non-Volatile Storage) and SPIFFS

### NVS Namespaces and Keys

**"timer" Namespace:**
- `gameDuration` (unsigned long, milliseconds)
- `breakDuration` (unsigned long, milliseconds)
- `numRounds` (unsigned int)
- `breakEnabled` (bool)
- `sirenLength` (unsigned long, milliseconds)
- `sirenPause` (unsigned long, milliseconds)
- `hcDefDur` (uint16_t) - Hello Club default round duration in minutes

**"users" Namespace:** (NEW in v3.0, updated in v3.1)
- `adminPass` (String) - Admin password hash (SHA-256; plaintext migrated on first login)
- `opCount` (int) - Number of operators
- `op_<N>_user` (String) - Operator N username
- `op_<N>_pass` (String) - Operator N password hash (SHA-256)

**"schedule" Namespace:** (NEW in v3.0)
- `enabled` (bool) - Global scheduling toggle
- `count` (int) - Number of schedules
- `sch_<N>_id` (String) - Schedule N ID
- `sch_<N>_club` (String) - Club name
- `sch_<N>_owner` (String) - Owner username
- `sch_<N>_day` (int) - Day of week (0-6)
- `sch_<N>_hour` (int) - Start hour (0-23)
- `sch_<N>_min` (int) - Start minute (0-59)
- `sch_<N>_dur` (int) - Duration in minutes
- `sch_<N>_en` (bool) - Schedule enabled flag
- `sch_<N>_time` (unsigned long) - Creation timestamp

**"helloclub" Namespace:** (NEW in v3.1)
- `apiKey` (String) - Hello Club API key
- `enabled` (bool) - Hello Club integration enabled flag
- `events` (String) - JSON-serialized cached events array (up to 20 events)
- `evt_cancel` (String) - Cancel flag: event ID of last manually-cancelled event (prevents boot recovery)

### SPIFFS Persistence

**Boot Log** (`/bootlog.txt`) - NEW in v3.1
- Timestamped boot entries with firmware version, reset reason, free heap
- WiFi connection details and scan results
- Boot recovery outcomes
- Auto-truncated at 8KB to prevent filling flash

**Load**: NVS loaded on boot (with defaults if unavailable); boot log appended on each boot

**Save**: When user changes settings, adds/removes users/schedules, or Hello Club events are fetched

---

## Error Handling

### Server-Side

- **SPIFFS failure**: Restart ESP32 after 5s
- **NVS failure**: Use defaults, log error
- **WiFi failure**: Fall back to captive portal
- **JSON overflow**: Send error to client with ERR_ code
- **Invalid input**: Validate and reject with descriptive error
- **Watchdog timeout**: Auto-restart ESP32
- **Rate limit exceeded**: Send ERR_RATE_LIMIT error (v3.0)
- **Session timeout**: Downgrade to viewer, send notification (v3.0)
- **Authentication failure**: Send ERR_AUTH_FAILED error (v3.0)
- **Permission denied**: Send error with proper message (v3.0)
- **Hello Club API failure**: Retry after 5 minutes, use cached events (v3.1)
- **HTTPS certificate failure**: Log error, skip fetch, retain cached events (v3.1)
- **Mid-event boot recovery**: Check cancel flag before resuming; log outcome (v3.1)

### Client-Side

- **WebSocket disconnect**: Exponential backoff reconnect
- **Authentication failure**: Prompt for password again
- **Session timeout**: Show notification, disable controls (v3.0)
- **Max reconnect attempts**: Show error, suggest page refresh
- **Invalid settings**: Show error banner
- **Permission denied**: Show error notification (v3.0)

---

## Security

### Threats Mitigated

| Threat | Mitigation |
|--------|------------|
| Unauthorized timer control | Role-based authentication |
| Unauthorized schedule modification | Owner-based permission checks |
| Privilege escalation | Server never accepts ownerUsername from client |
| Password theft from NVS | SHA-256 hashing with automatic plaintext migration (v3.1) |
| Unauthorized firmware updates | OTA password protection |
| Input validation attacks | Server-side range checking |
| XSS attacks | HTML escaping of user-supplied strings (v3.1) |
| Replay attacks | Session-based authentication |
| Brute force attacks | Rate limiting (10 msg/sec) |
| Session hijacking | 30-minute timeout |
| Man-in-the-middle (HC API) | HTTPS with pinned root CA certificates (v3.1) |
| Network eavesdropping | Use on trusted local network only |

### Security Best Practices

- Change default admin password immediately
- Use strong passwords for operators (min 5 chars)
- Regular password changes recommended
- Monitor serial output for suspicious auth attempts
- Keep WiFi network secured (WPA2/WPA3)
- Only expose timer on trusted networks
- Regularly update firmware
- Use factory reset if credentials compromised
- Review boot log (`/bootlog.txt`) for unexpected resets

---

## Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| WebSocket latency | 5-50ms | Local network |
| Timer precision | +/-10ms | Server-side |
| Display update rate | 60fps | Client-side |
| Sync interval | 5 seconds | Timer and NTP status |
| Session timeout | 30 minutes | Inactivity-based |
| Rate limit | 10 msg/sec | Per client |
| Schedule check interval | 30 seconds | Auto-trigger detection |
| HC event check interval | 30 seconds | When HC enabled |
| HC API poll interval | 1 hour | With 5-min retry on failure |
| HC trigger window | 2 minutes | Window around event start |
| Max schedules | 50 | Configurable |
| Max operators | 10 | Configurable |
| Max cached HC events | 20 | Configurable |
| Max concurrent clients | 10 | WebSocket limit |
| Memory usage (ESP32) | ~60KB RAM | With all features |
| Flash usage (ESP32) | ~1.2MB | Program code |
| SPIFFS usage | ~200KB | Web interface files + boot log |
| NVS usage | ~4KB | Settings, users, schedules, HC cache |
| Watchdog timeout | 30 seconds | Auto-restart |
| Schedule trigger cooldown | 2 minutes | Prevents re-trigger |

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
- Authentication attempts (username redacted) (v3.0)
- Rate limiting violations (v3.0)
- Session timeout events (v3.0)
- Schedule trigger checks (v3.0)
- NTP sync status changes (v3.0)
- Hello Club API fetch results and errors (v3.1)
- Hello Club event trigger and cutoff events (v3.1)
- Mid-event boot recovery outcomes (v3.1)
- Timer state changes
- Settings load/save operations
- Siren activation sequences

### Boot Log (NEW in v3.1)

The boot logger writes to `/bootlog.txt` on SPIFFS. Each boot appends:
- Boot separator and timestamp
- Firmware version and build date
- ESP32 reset reason (power-on, watchdog, crash, etc.)
- Free heap at boot
- WiFi scan results and connection outcome
- Boot recovery decisions (resumed, skipped, or no recovery needed)

The log auto-truncates at 8KB. Read via serial monitor or by downloading from SPIFFS.

### Common Issues (Updated for v3.1)

**Timer not syncing:**
- Check WebSocket connection status (green dot)
- Check serial output for sync broadcasts
- Verify client receives sync messages (browser console)

**Can't login:**
- Check credentials (case-sensitive)
- Verify operator account exists (admin must create it)
- Check serial monitor for auth errors
- Try factory reset (admin only)

**Schedules not auto-starting:**
- Check NTP sync status (must be synced)
- Verify scheduling enabled (toggle switch ON)
- Check schedule time matches current Pacific/Auckland time
- Wait 2 minutes after last trigger (cooldown period)

**Hello Club events not triggering:**
- Verify Hello Club integration is enabled in settings
- Verify API key is configured and valid
- Check that events have "timer:" tag in their description
- Check NTP sync status (must be synced for time comparison)
- Review serial output for HC fetch errors or certificate failures
- Check `/bootlog.txt` for connection issues

**Boot recovery not working:**
- Check that the event was not manually cancelled before reboot (cancel flag)
- Verify event end time has not passed
- Review boot log for recovery decision details

**Session keeps timing out:**
- Timeout is 30 minutes of inactivity
- Any WebSocket message resets timer
- Adjust SESSION_TIMEOUT in config.h if needed

**Android WiFi portal issues:**
- Disable "Smart Network Switch" in Android settings
- Tap "Stay connected" when prompted
- Try forgetting network and reconnecting

**Watchdog resets:**
- Check for blocking code in main loop
- Verify no long delays
- Review serial output and boot log before reset

---

## Future Enhancements

Potential improvements for future versions:
- Multi-court support (multiple timers)
- Statistics tracking (matches played, total time, usage per club)
- Email/SMS notifications for schedule reminders
- Mobile app (React Native / Flutter)
- Cloud backup of settings and schedules
- RESTful API in addition to WebSocket
- LCD display support for local viewing without browser
- BLE control for offline operation
- Schedule conflict detection and warnings
- Recurring schedule templates
- Holiday/exception handling for schedules
- Advanced analytics dashboard for admins
- Two-factor authentication
- Audit logging for admin actions

---

## References

- **ESP32 Documentation**: https://docs.espressif.com/projects/esp-idf/
- **ArduinoJson**: https://arduinojson.org/
- **ESPAsyncWebServer**: https://github.com/me-no-dev/ESPAsyncWebServer
- **ezTime**: https://github.com/ropg/ezTime
- **Hello Club API**: https://api.helloclub.com
- **Web Audio API**: https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API
- **requestAnimationFrame**: https://developer.mozilla.org/en-US/docs/Web/API/window/requestAnimationFrame
- **CSS Grid Layout**: https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Grid_Layout

---

**Last Updated**: 2026-03-18
**Document Version**: 3.1
**Project**: ESP32 Badminton Timer
