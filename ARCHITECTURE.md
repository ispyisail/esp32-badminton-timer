# ESP32 Badminton Timer - System Architecture

## Overview

The ESP32 Badminton Timer is a web-controlled timer system designed for badminton courts with multi-club scheduling and role-based authentication. It uses a client-server architecture with real-time synchronization via WebSockets.

**Version**: 3.0.0
**Last Updated**: 2025-10-30

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
│  │  ┌──────────┐  ┌──────────┐                          │  │
│  │  │   User   │  │ Schedule │                          │  │
│  │  │ Manager  │  │ Manager  │                          │  │
│  │  └──────────┘  └──────────┘                          │  │
│  │                                                        │  │
│  │  ┌────────────────────────────────────────────────┐  │  │
│  │  │        WebSocket Server (ws://.../ws)         │  │  │
│  │  │  • Authentication                              │  │  │
│  │  │  • Rate Limiting (10 msg/sec)                 │  │  │
│  │  │  • Session Timeout (30 min)                   │  │  │
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
│  │  │  • ezTime (NTP Client)                         │  │  │
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
│  │  │  • Schedule Manager                            │  │  │
│  │  │  • Calendar Renderer (CSS Grid)                │  │  │
│  │  │  • UI Controller                               │  │  │
│  │  │  • Keyboard Shortcut Handler                   │  │  │
│  │  │  • Sound Effects (Web Audio API)               │  │  │
│  │  └────────────────────────────────────────────────┘  │  │
│  │                                                        │  │
│  │  ┌────────────────────────────────────────────────┐  │  │
│  │  │           CSS Styling (style.css)              │  │  │
│  │  │  • Login Modal                                 │  │  │
│  │  │  • User Management Page                        │  │  │
│  │  │  • Schedule Page with Weekly Calendar          │  │  │
│  │  │  • Role-Based Visibility (CSS classes)         │  │  │
│  │  └────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## Component Breakdown

### Server-Side (ESP32)

#### 1. Core Modules

**User Manager** (`users.h/cpp`) - NEW in v3.0
- Three-tier role-based authentication (VIEWER, OPERATOR, ADMIN)
- NVS persistence for operators
- Default admin account (admin/admin)
- Password validation (minimum 4 characters)
- Factory reset capability
- State: Admin password (runtime), Operators list (NVS)

**Schedule Manager** (`schedule.h/cpp`) - NEW in v3.0
- Weekly recurring schedule management
- Club-based organization with owner tracking
- Week-minute calculation (0-10079 minutes from Sunday 00:00)
- Auto-trigger detection with 2-minute cooldown
- Permission checking (operators see only their schedules)
- Up to 50 schedules stored in NVS
- Unique ID generation with collision detection
- State machine: `ENABLED/DISABLED` global toggle

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
- **Authentication enforcement (v3.0)**
- **Rate limiting (10 messages/second per client) (v3.0)**
- **Session timeout (30 minutes inactivity) (v3.0)**
- Message routing and validation
- HTTP server for static files
- Captive portal for WiFi setup
- mDNS service advertising
- **NTP status monitoring and broadcasting (v3.0)**
- **Schedule auto-triggering (v3.0)**

**Key Features:**
- Watchdog timer (30s timeout, auto-restart on hang)
- Self-test (SPIFFS, NVS, Relay verification)
- Debug mode with configurable logging
- **Per-client authentication tracking with username storage (v3.0)**
- **Per-client rate limiting with sliding window (v3.0)**
- **Session timeout with automatic viewer downgrade (v3.0)**
- Server-side input validation
- Periodic timer sync broadcasts (every 5s)
- **NTP sync status checks (every 5s) (v3.0)**
- **Schedule trigger checks (every minute) (v3.0)**
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

#### 5. Security (Enhanced in v3.0)

**WebSocket Authentication:**
- Role-based access control (VIEWER, OPERATOR, ADMIN)
- Per-client authentication state
- **Username tracking for schedule ownership (v3.0)**
- **Session timeout after 30 minutes inactivity (v3.0)**
- **Rate limiting (10 messages/second per client) (v3.0)**
- **Server-side ownership enforcement - client cannot spoof ownerUsername (v3.0)**

**Permission Hierarchy:**
- VIEWER: View only
- OPERATOR: View + Control timer + Manage own schedules
- ADMIN: Everything + User management + View all schedules

**OTA Protection:**
- Password required (`OTA_PASSWORD`)
- Hostname validation

**Input Validation:**
- All schedule fields validated (day 0-6, hour 0-23, minute 0-59)
- All settings ranges checked
- Minimum password length (4 characters)
- Error codes with ERR_ prefix

---

### Client-Side (Web Browser)

#### 1. Authentication Flow (NEW in v3.0)

**Login Process:**
```
User opens page
    ↓
Login modal displayed
    ↓
User chooses:
├─ Enter credentials (admin/operator)
│   ↓
│   Send authenticate action with username/password
│   ↓
│   Server validates against UserManager
│   ↓
│   Success: Receive auth_success, set userRole, hide modal
│   Failure: Show error message
│
└─ Click "Continue as Viewer"
    ↓
    Send authenticate with empty credentials
    ↓
    Server grants VIEWER role
    ↓
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
saveSchedule() → send add_schedule with nested { schedule: {...} }
receive schedule_added → add to local array, re-render calendar

// Read
on page load → send get_schedules
receive schedules_list → store in local array, render calendar

// Update
editSchedule(id) → populate form
saveSchedule() → send update_schedule with nested { schedule: {...} }
receive schedule_updated → update local array, re-render

// Delete
deleteSchedule(id) → send delete_schedule
receive schedule_deleted → remove from local array, re-render
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
  │                               │
  ├─── Connect ───────────────────▶
  │                               │
  ◀─── State ─────────────────────┤
  │                               │
  ├─── Action: authenticate ─────▶ (username + password)
  │                               │
  ◀─── Event: auth_success ───────┤
  ◀─── Event: schedules_list ─────┤
  │                               │
  ├─── Action: start ─────────────▶ (authenticated)
  │                               │
  ◀─── Event: start ──────────────┤ (broadcast to all)
  ◀─── Event: sync ───────────────┤ (every 5s)
  ◀─── Event: sync ───────────────┤
  │                               │
  ├─── Action: add_schedule ──────▶ (operator/admin only)
  │                               │
  ◀─── Event: schedule_added ─────┤
  │                               │
  ◀─── Event: session_timeout ────┤ (after 30 min inactivity)
  │                               │
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
- **NTP sync status (✓/✗/⏳) (v3.0)**
- **Role badge (Viewer/Operator/Admin) (v3.0)**
- Button disabled states during processing
- Loading overlay on connect
- Temporary notification banners
- Smooth animations (fade in/out)

**Sound Effects** (optional, toggle with `M`):
- Start: 880 Hz beep
- Pause/Resume: 660 Hz beep
- Reset: 440 Hz beep
- Implemented with Web Audio API

**New Pages (v3.0):**
- **Login Modal**: Credentials input with "Continue as Viewer" option
- **User Management Page**: Add/remove operators (admin only)
- **Schedule Page**: Weekly calendar grid, add/edit/delete schedules

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
    ↓
Client: Disable buttons, send { action: "authenticate", username, password }
    ↓
Server: Validate credentials via UserManager
    ↓
Server: If admin/admin → role = ADMIN
        If operator found → role = OPERATOR
        If empty credentials → role = VIEWER
        Else → send error
    ↓
Server: Store authenticatedClients[clientId] = role
        Store authenticatedUsernames[clientId] = username
    ↓
Server: Send { event: "auth_success", role, username }
    ↓
Client: Set userRole, currentUsername
        Hide login modal
        Update UI for role (hide/show elements)
        Enable controls (if not viewer)
```

### 2. Schedule Creation Sequence (NEW in v3.0)

```
Operator fills schedule form
    ↓
Client: Send { action: "add_schedule", schedule: {clubName, dayOfWeek, ...} }
    ↓
Server: Check authentication (reject if VIEWER)
    ↓
Server: Generate unique schedule ID (timestamp + counter)
    ↓
Server: Set ownerUsername = authenticatedUsernames[clientId] (NOT from client!)
    ↓
Server: Validate all fields (day 0-6, hour 0-23, etc.)
    ↓
Server: If invalid → send error, return
    ↓
Server: scheduleManager.addSchedule()
    ↓
Server: Save to NVS
    ↓
Server: Send { event: "schedule_added", schedule: {...} } to requester
    ↓
Client: Add schedule to local array
        Re-render calendar
        Show "Schedule added" notification
```

### 3. Schedule Auto-Trigger Sequence (NEW in v3.0)

```
Server: Every 60 seconds in loop()
    ↓
Server: Check scheduleManager.checkScheduleTrigger()
    ↓
Server: Calculate current week-minute (0-10079)
        For each enabled schedule:
            Calculate schedule's week-minute
            If match && not triggered in last 2 minutes:
                ↓
                Server: Start timer with schedule duration
                ↓
                Server: Broadcast { event: "start", ... }
                ↓
                All Clients: Timer starts automatically
                ↓
                Server: Record lastTriggerTime for this schedule
```

### 4. Session Timeout Sequence (NEW in v3.0)

```
Server: Every 60 seconds in loop()
    ↓
Server: For each connected client:
        Check time since clientLastActivity[clientId]
    ↓
Server: If > 30 minutes && role != VIEWER:
        ↓
        Server: Set authenticatedClients[clientId] = VIEWER
                Remove authenticatedUsernames[clientId]
        ↓
        Server: Send { event: "session_timeout" } to client
    ↓
Client: Set userRole = 'viewer'
        Update UI (disable controls)
        Show notification: "Session expired - Continue as viewer or login again"
```

### 5. NTP Status Update Sequence (NEW in v3.0)

```
Server: Every 5 seconds in loop()
    ↓
Server: Check if ezTime sync status changed
    ↓
Server: Validate sync (year 2020-2100, month 1-12, day 1-31)
    ↓
Server: If status changed:
        ↓
        Server: Broadcast { event: "ntp_status", synced, time, ... }
    ↓
All Clients: Update NTP indicator
             If synced: Show ✓ (green)
             If syncing: Show ⏳ (amber, pulsing)
             If error: Show ✗ (red, pulsing)
```

### 6. Timer Start Sequence (Updated for v3.0)

```
User clicks "Start"
    ↓
Client: Disable buttons, play sound, send { action: "start" }
    ↓
Server: Check authentication (reject if VIEWER or not authenticated)
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

---

## State Machines

### Authentication State Machine (NEW in v3.0)

```
┌──────────────┐
│  NO_AUTH     │
│ (connected)  │
└──────────────┘
        │
        │ authenticate (empty credentials)
        ├────────────────────┐
        │                    ▼
        │              ┌──────────┐
        │              │  VIEWER  │
        │              └──────────┘
        │
        │ authenticate (credentials)
        ├─────────────────────┐
        │                     │
        ▼                     ▼
┌──────────────┐      ┌──────────────┐
│   OPERATOR   │      │    ADMIN     │
└──────────────┘      └──────────────┘
        │                     │
        │ session_timeout     │ session_timeout
        └─────────┬───────────┘
                  │
                  ▼
            ┌──────────┐
            │  VIEWER  │
            └──────────┘
```

### Schedule System State Machine (NEW in v3.0)

```
┌────────────────┐
│ SCHEDULING OFF │
└────────────────┘
        │
        │ set_scheduling(enabled: true)
        ▼
┌────────────────┐
│ SCHEDULING ON  │◀─────┐
└────────────────┘      │
        │               │
        │ Every minute  │
        ▼               │
┌────────────────┐      │
│ CHECK TRIGGERS │      │
└────────────────┘      │
        │               │
        │ Match found?  │
        ├─ NO ──────────┘
        │
        └─ YES
           ↓
    ┌────────────────┐
    │ START TIMER    │
    │ (auto-trigger) │
    └────────────────┘
```

### Timer State Machine (Unchanged from v2.0)

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

### WebSocket Connection State Machine (Updated for v3.0)

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
│ (no auth)    │  │ (exp backoff)│
└──────────────┘  └──────────────┘
        │             ▲     │
        │ authenticate│     │ max attempts
        ▼             │     ▼
┌──────────────┐     │  ┌──────────────┐
│AUTHENTICATED │─────┘  │    FAILED    │
│(viewer/op/ad)│        └──────────────┘
└──────────────┘
        │
        │ session_timeout (auto-downgrade)
        ▼
┌──────────────┐
│AUTHENTICATED │
│  (viewer)    │
└──────────────┘
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

**Storage**: ESP32 NVS (Non-Volatile Storage)

### Namespaces and Keys

**"timer" Namespace:**
- `gameDuration` (unsigned long, milliseconds)
- `breakDuration` (unsigned long, milliseconds)
- `numRounds` (unsigned int)
- `breakEnabled` (bool)
- `sirenLength` (unsigned long, milliseconds)
- `sirenPause` (unsigned long, milliseconds)

**"users" Namespace:** (NEW in v3.0)
- `adminPass` (String) - Admin password
- `opCount` (int) - Number of operators
- `op_<N>_user` (String) - Operator N username
- `op_<N>_pass` (String) - Operator N password

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

**Load**: On boot (with defaults if NVS unavailable)

**Save**: When user changes settings, adds/removes users/schedules

---

## Error Handling

### Server-Side

- **SPIFFS failure**: Restart ESP32 after 5s
- **NVS failure**: Use defaults, log error
- **WiFi failure**: Fall back to captive portal
- **JSON overflow**: Send error to client with ERR_ code
- **Invalid input**: Validate and reject with descriptive error
- **Watchdog timeout**: Auto-restart ESP32
- **Rate limit exceeded**: Send ERR_RATE_LIMIT error (NEW in v3.0)
- **Session timeout**: Downgrade to viewer, send notification (NEW in v3.0)
- **Authentication failure**: Send ERR_AUTH_FAILED error (NEW in v3.0)
- **Permission denied**: Send error with proper message (NEW in v3.0)

### Client-Side

- **WebSocket disconnect**: Exponential backoff reconnect
- **Authentication failure**: Prompt for password again
- **Session timeout**: Show notification, disable controls (NEW in v3.0)
- **Max reconnect attempts**: Show error, suggest page refresh
- **Invalid settings**: Show error banner
- **Permission denied**: Show error notification (NEW in v3.0)

---

## Security

### Threats Mitigated

| Threat | Mitigation |
|--------|------------|
| Unauthorized timer control | Role-based authentication |
| Unauthorized schedule modification | Owner-based permission checks |
| Privilege escalation | Server never accepts ownerUsername from client |
| Unauthorized firmware updates | OTA password protection |
| Input validation attacks | Server-side range checking |
| Replay attacks | Session-based authentication |
| Brute force attacks | Rate limiting (10 msg/sec) |
| Session hijacking | 30-minute timeout |
| Network eavesdropping | Use on trusted local network only |

### Security Best Practices (v3.0)

- **Change default admin password immediately**
- **Use strong passwords for operators (min 4 chars)**
- **Regular password changes recommended**
- **Monitor serial output for suspicious auth attempts**
- **Keep WiFi network secured (WPA2/WPA3)**
- **Only expose timer on trusted networks**
- **Regularly update firmware**
- **Use factory reset if credentials compromised**

---

## Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| WebSocket latency | 5-50ms | Local network |
| Timer precision | ±10ms | Server-side |
| Display update rate | 60fps | Client-side |
| Sync interval | 5 seconds | Timer & NTP status |
| Session timeout | 30 minutes | Inactivity-based |
| Rate limit | 10 msg/sec | Per client |
| Schedule check interval | 60 seconds | Auto-trigger detection |
| Max schedules | 50 | Configurable |
| Max operators | 10 | Configurable |
| Max concurrent clients | 10 | WebSocket limit |
| Memory usage (ESP32) | ~60KB RAM | With all features |
| Flash usage (ESP32) | ~1.2MB | Program code |
| SPIFFS usage | ~200KB | Web interface files |
| NVS usage | ~4KB | Settings, users, schedules |
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
- **Authentication attempts (username redacted) (v3.0)**
- **Rate limiting violations (v3.0)**
- **Session timeout events (v3.0)**
- **Schedule trigger checks (v3.0)**
- **NTP sync status changes (v3.0)**
- Timer state changes
- Settings load/save operations
- Siren activation sequences

### Common Issues (Updated for v3.0)

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
- Check NTP sync status (must show green ✓)
- Verify scheduling enabled (toggle switch ON)
- Check schedule time matches current Pacific/Auckland time
- Wait 2 minutes after last trigger (cooldown period)

**Session keeps timing out:**
- Timeout is 30 minutes of inactivity
- Any WebSocket message resets timer
- Adjust SESSION_TIMEOUT in main.cpp if needed

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

Potential improvements for v4.0:
- HTTPS support with self-signed certificates
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
- **Web Audio API**: https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API
- **requestAnimationFrame**: https://developer.mozilla.org/en-US/docs/Web/API/window/requestAnimationFrame
- **CSS Grid Layout**: https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Grid_Layout

---

**Last Updated**: 2025-10-30
**Document Version**: 3.0
**Author**: Claude Code
**Project**: ESP32 Badminton Timer
