# ESP32 Badminton Timer - WebSocket API Documentation

## Overview

The ESP32 Badminton Timer uses WebSockets for real-time bidirectional communication between the server (ESP32) and clients (web browsers).

**WebSocket Endpoint**: `ws://badminton-timer.local/ws` or `ws://<ESP32_IP>/ws`

**Protocol**: JSON-based messages

**Authentication**: Role-based access control (VIEWER, OPERATOR, ADMIN)

**Version**: 3.2.0

---

## Connection Flow

```
1. Client connects to WebSocket endpoint
2. Server sends { event: "login_prompt", message: "..." }
3. Server sends { event: "state", state: {...} }
4. Client automatically starts in VIEWER mode (no login required)
5. Optional: Client sends { action: "authenticate", username, password }
6. Server validates credentials via UserManager
7. If valid: Server sends { event: "auth_success" } or { event: "viewer_mode" }
8. If invalid: Server sends { event: "error", message: "ERR_AUTH_FAILED: ..." }
```

---

## Message Format

All messages are JSON objects with an `event` (server->client) or `action` (client->server) field.

---

## Client -> Server (Actions)

### Authentication Actions

#### authenticate
**Purpose**: Authenticate the WebSocket connection with username and password

```json
{
  "action": "authenticate",
  "username": "admin",
  "password": "admin"
}
```

**Viewer Mode** (no credentials required):
```json
{
  "action": "authenticate",
  "username": "",
  "password": ""
}
```

**Responses**:
- `auth_success` (for admin/operator)
- `viewer_mode` (for empty credentials)
- `error` with `ERR_AUTH_FAILED`

**Roles**:
- **ADMIN**: username="admin", password=configured admin password
- **OPERATOR**: username from operators list, password from operators list
- **VIEWER**: empty username and password

---

### Timer Control Actions

#### start
**Purpose**: Start the timer from IDLE or FINISHED state

```json
{
  "action": "start"
}
```

**Permission**: OPERATOR or ADMIN required
**Validation**: Timer must be in IDLE or FINISHED state
**Response**: `start` event broadcast to all clients

**Errors**:
- "Timer already active. Reset first." (if RUNNING/PAUSED)
- "Permission denied - viewer mode" (if VIEWER)

---

#### pause
**Purpose**: Pause timer if RUNNING, or resume if PAUSED

```json
{
  "action": "pause"
}
```

**Permission**: OPERATOR or ADMIN required
**Response**: `pause` or `resume` event

---

#### reset
**Purpose**: Reset timer to IDLE state

```json
{
  "action": "reset"
}
```

**Permission**: OPERATOR or ADMIN required
**Response**: `reset` event broadcast to all clients

---

#### pause_after_next
**Purpose**: Set a one-shot flag to automatically pause between rounds

```json
{
  "action": "pause_after_next",
  "enabled": true
}
```

**Permission**: OPERATOR or ADMIN required
**Response**: `pause_after_next_changed` event broadcast to all clients

**Behavior**: When enabled, the timer will pause at the end of the current round instead of continuing to the next round or break. The flag is cleared after it triggers.

---

#### save_settings
**Purpose**: Update timer settings

```json
{
  "action": "save_settings",
  "settings": {
    "gameDuration": 21,
    "breakDuration": 60,
    "numRounds": 3,
    "breakTimerEnabled": true,
    "sirenLength": 1000,
    "sirenPause": 1000
  }
}
```

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `gameDuration` | number | 1-120 | Game duration in minutes |
| `breakDuration` | number | 1-3600 | Break duration in seconds |
| `numRounds` | number | 1-20 | Number of rounds |
| `breakTimerEnabled` | boolean | | Enable break timer between rounds |
| `sirenLength` | number | 100-10000 | Siren on duration in milliseconds |
| `sirenPause` | number | 100-10000 | Siren off duration in milliseconds |

**Permission**: ADMIN only
**Response**: `settings` event broadcast to all clients

---

### Timezone Actions

#### set_timezone
**Purpose**: Set the system timezone

```json
{
  "action": "set_timezone",
  "timezone": "Pacific/Auckland"
}
```

**Permission**: ADMIN only
**Value**: IANA timezone string (e.g. "Pacific/Auckland", "Australia/Sydney")
**Response**: `timezone_changed` event sent to requester

---

### Schedule Management Actions

#### get_schedules
**Purpose**: Retrieve all schedules (filtered by role)

```json
{
  "action": "get_schedules"
}
```

**Permission**: Any authenticated user
**Response**: `schedules_list` event

**Filtering**:
- ADMIN: Receives all schedules from all clubs
- OPERATOR: Receives only schedules where `ownerUsername` matches their username
- VIEWER: Receives all schedules (read-only)

---

#### add_schedule
**Purpose**: Create a new schedule

```json
{
  "action": "add_schedule",
  "schedule": {
    "clubName": "Badminton Club A",
    "dayOfWeek": 1,
    "startHour": 18,
    "startMinute": 30,
    "durationMinutes": 90,
    "enabled": true
  }
}
```

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `clubName` | string | non-empty | Club/group name |
| `dayOfWeek` | number | 0-6 | 0=Sunday, 1=Monday, ..., 6=Saturday |
| `startHour` | number | 0-23 | Start hour |
| `startMinute` | number | 0-59 | Start minute |
| `durationMinutes` | number | >0 | Duration in minutes |
| `enabled` | boolean | | Whether this schedule is active |

**Permission**: OPERATOR or ADMIN required

**Server Behavior**:
- Generates unique schedule ID (timestamp + counter)
- Sets `ownerUsername` to authenticated user's username (NOT from client!)
- Sets `createdAt` timestamp

**Response**: `schedule_added` event to requester only

**Errors**:
- "Permission denied - viewer mode"
- "Invalid day of week (must be 0-6)"
- "Invalid hour (must be 0-23)"
- "Invalid minute (must be 0-59)"
- "Missing schedule data"

---

#### update_schedule
**Purpose**: Update an existing schedule

```json
{
  "action": "update_schedule",
  "schedule": {
    "id": "1698765432-1",
    "clubName": "Badminton Club A",
    "dayOfWeek": 2,
    "startHour": 19,
    "startMinute": 0,
    "durationMinutes": 120,
    "enabled": true
  }
}
```

**Permission**: OPERATOR (own schedules only) or ADMIN (all schedules)
**Validation**: Same as `add_schedule` plus:
- Schedule with `id` must exist
- OPERATOR can only update schedules where `ownerUsername` matches their username
- ADMIN can update any schedule

**Server Behavior**:
- Preserves `ownerUsername` from existing schedule (client cannot change ownership!)
- Preserves `createdAt` timestamp
- Updates all other fields from client

**Response**: `schedule_updated` event to requester only

**Errors**:
- "Schedule not found"
- "Permission denied - you can only edit your own schedules"
- Validation errors same as `add_schedule`

---

#### delete_schedule
**Purpose**: Delete a schedule

```json
{
  "action": "delete_schedule",
  "id": "1698765432-1"
}
```

**Permission**: OPERATOR (own schedules only) or ADMIN (all schedules)
**Response**: `schedule_deleted` event to requester only

**Errors**:
- "Schedule not found"
- "Permission denied - you can only delete your own schedules"

---

#### set_scheduling
**Purpose**: Enable or disable the automatic scheduling system

```json
{
  "action": "set_scheduling",
  "enabled": true
}
```

**Permission**: OPERATOR or ADMIN required
**Response**: `scheduling_status` event broadcast to all clients

**Behavior**:
- When `enabled=true`: ESP32 checks every minute if any schedule should trigger
- When `enabled=false`: Schedules are not checked, timer must be started manually

---

### User Management Actions

#### get_operators
**Purpose**: Retrieve list of all operator accounts

```json
{
  "action": "get_operators"
}
```

**Permission**: ADMIN only
**Response**: `operators_list` event

---

#### add_operator
**Purpose**: Create a new operator account

```json
{
  "action": "add_operator",
  "username": "newoperator",
  "password": "securepass123"
}
```

**Permission**: ADMIN only
**Validation**:
- `username`: Required, non-empty, unique
- `password`: Minimum 4 characters
- Maximum 10 operators

**Response**: `operator_added` event

**Errors**:
- "Permission denied - admin only"
- "Username already exists"
- "Password must be at least 4 characters"
- "Maximum number of operators reached"

---

#### remove_operator
**Purpose**: Delete an operator account

```json
{
  "action": "remove_operator",
  "username": "operator1"
}
```

**Permission**: ADMIN only
**Response**: `operator_removed` event

**Errors**:
- "Permission denied - admin only"
- "Operator not found"

**Note**: This does NOT delete schedules created by that operator. Schedules remain with their `ownerUsername`.

---

#### change_password
**Purpose**: Change password for a user account

```json
{
  "action": "change_password",
  "username": "operator1",
  "oldPassword": "currentpass",
  "newPassword": "newpass123"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `username` | string | The username whose password to change |
| `oldPassword` | string | Must match the current password |
| `newPassword` | string | Minimum 4 characters for operators, no minimum for admin |

**Permission**: OPERATOR or ADMIN
**Response**: `password_changed` event

**Errors**:
- "ERR_PASSWORD_CHANGE: Old password is incorrect"
- "Password must be at least 4 characters"

---

#### factory_reset
**Purpose**: Reset all settings, users, and schedules to defaults

```json
{
  "action": "factory_reset"
}
```

**Permission**: ADMIN only
**Behavior**:
- Deletes all operator accounts
- Deletes all schedules
- Resets admin password to "admin"
- Disables scheduling
- Resets all settings to defaults

**Response**: `factory_reset_complete` event

**Warning**: This is irreversible! All data is permanently deleted.

---

### Hello Club Integration Actions

#### get_upcoming_events
**Purpose**: Get cached Hello Club events

```json
{
  "action": "get_upcoming_events"
}
```

**Permission**: Any authenticated user
**Response**: `upcoming_events` event

---

#### get_helloclub_settings
**Purpose**: Get Hello Club API configuration

```json
{
  "action": "get_helloclub_settings"
}
```

**Permission**: ADMIN only
**Response**: `helloclub_settings` event

---

#### save_helloclub_settings
**Purpose**: Save Hello Club API configuration

```json
{
  "action": "save_helloclub_settings",
  "apiKey": "your-api-key-here",
  "enabled": true,
  "defaultDuration": 12
}
```

| Field | Type | Description |
|-------|------|-------------|
| `apiKey` | string | Hello Club API key |
| `enabled` | boolean | Enable Hello Club integration |
| `defaultDuration` | number | Default event duration in minutes |

**Permission**: ADMIN only
**Response**: `helloclub_settings_saved` event

---

#### helloclub_refresh
**Purpose**: Force-refresh Hello Club events from the API

```json
{
  "action": "helloclub_refresh"
}
```

**Permission**: OPERATOR or ADMIN
**Response**: `helloclub_refresh_result` event

---

### QR Code Actions

#### get_qr_config
**Purpose**: Get QR code WiFi connection settings

```json
{
  "action": "get_qr_config"
}
```

**Permission**: Any authenticated user
**Response**: `qr_config` event

---

#### save_qr_settings
**Purpose**: Save QR code WiFi connection settings

```json
{
  "action": "save_qr_settings",
  "password": "wifi-password",
  "encryption": "WPA",
  "ssid": "MyNetwork"
}
```

| Field | Type | Values | Description |
|-------|------|--------|-------------|
| `ssid` | string | | WiFi SSID override (optional) |
| `password` | string | | WiFi password |
| `encryption` | string | "WPA", "WEP", "nopass" | WiFi encryption type |

**Permission**: ADMIN only
**Response**: `qr_settings_saved` event

---

## Server -> Client (Events)

### Authentication Events

#### login_prompt
**When**: Immediately on WebSocket connection

```json
{
  "event": "login_prompt",
  "message": "Please log in or continue as viewer"
}
```

**Purpose**: Prompt the client to authenticate
**Client Action**: Display login form or auto-connect as viewer

---

#### state
**When**: Client connects, or timer state changes

```json
{
  "event": "state",
  "state": {
    "status": "IDLE",
    "mainTimer": 1260000,
    "breakTimer": 60000,
    "currentRound": 1,
    "numRounds": 3,
    "time": "10:45:30 AM"
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | "IDLE", "RUNNING", "PAUSED", or "FINISHED" |
| `mainTimer` | number | Milliseconds remaining on main timer |
| `breakTimer` | number | Milliseconds remaining on break timer |
| `currentRound` | number | Current round (1-indexed) |
| `numRounds` | number | Total number of rounds |
| `time` | string | Current time in configured timezone |

**Purpose**: Send full state snapshot
**Client Action**: Update UI to reflect current state
**Note**: Sent immediately on connection, no authentication required

---

#### auth_success
**When**: Successful authentication as ADMIN or OPERATOR

```json
{
  "event": "auth_success",
  "role": "admin",
  "username": "admin"
}
```

**Purpose**: Notify client of successful login
**Client Action**:
- Store `role` and `username`
- Hide login modal
- Enable controls based on role
- Update UI to show username

---

#### viewer_mode
**When**: Successful authentication with empty credentials

```json
{
  "event": "viewer_mode",
  "role": "viewer",
  "username": "Viewer"
}
```

**Purpose**: Notify client they're in view-only mode
**Client Action**:
- Disable all control buttons
- Hide admin-only elements
- Show "Viewer" badge

---

#### session_timeout
**When**: 30 minutes of inactivity (no WebSocket messages sent)

```json
{
  "event": "session_timeout"
}
```

**Purpose**: Notify client their session expired
**Client Action**:
- Downgrade role to "viewer"
- Disable controls
- Show notification: "Session expired - Login again or continue as viewer"

**Note**: Only sent to ADMIN/OPERATOR users. VIEWER mode has no timeout.

---

### Timer Events

#### start
**When**: Timer starts from IDLE or FINISHED

```json
{
  "event": "start",
  "gameDuration": 1260000,
  "breakDuration": 60000,
  "numRounds": 3,
  "currentRound": 1,
  "continuousMode": false,
  "pauseAfterNext": false
}
```

| Field | Type | Description |
|-------|------|-------------|
| `gameDuration` | number | Total game duration in milliseconds |
| `breakDuration` | number | Total break duration in milliseconds |
| `numRounds` | number | Total number of rounds |
| `currentRound` | number | Current round (1-indexed) |
| `continuousMode` | boolean | Whether break timer is disabled |
| `pauseAfterNext` | boolean | Whether pause-after-next flag is set |

**Sent to**: All clients (broadcast)
**Client Action**: Start requestAnimationFrame loop, reset timer display

---

#### sync
**When**: Timer is RUNNING or PAUSED, sent every 5 seconds

```json
{
  "event": "sync",
  "mainTimerRemaining": 1234567,
  "breakTimerRemaining": 45678,
  "serverMillis": 98765432,
  "currentRound": 2,
  "numRounds": 3,
  "status": "RUNNING"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `mainTimerRemaining` | number | Milliseconds remaining on main timer |
| `breakTimerRemaining` | number | Milliseconds remaining on break timer |
| `serverMillis` | number | Server's `millis()` timestamp |
| `currentRound` | number | Current round (1-indexed) |
| `numRounds` | number | Total number of rounds |
| `status` | string | "RUNNING" or "PAUSED" |

**Sent to**: All clients (broadcast)
**Client Action**: Update sync baseline, calculate current time client-side

---

#### pause
**When**: Timer paused

```json
{
  "event": "pause",
  "mainTimerRemaining": 1234567
}
```

| Field | Type | Description |
|-------|------|-------------|
| `mainTimerRemaining` | number | Milliseconds remaining on main timer at time of pause |

---

#### resume
**When**: Timer resumed

```json
{
  "event": "resume",
  "mainTimerRemaining": 1234567
}
```

| Field | Type | Description |
|-------|------|-------------|
| `mainTimerRemaining` | number | Milliseconds remaining on main timer at time of resume |

---

#### pause_after_next_changed
**When**: Pause-after-next flag is toggled

```json
{
  "event": "pause_after_next_changed",
  "enabled": true
}
```

**Sent to**: All clients (broadcast)
**Client Action**: Update pause-after-next toggle/indicator

---

#### reset
**When**: Timer reset to IDLE

```json
{
  "event": "reset"
}
```

---

#### finished
**When**: All rounds complete

```json
{
  "event": "finished"
}
```

---

#### new_round
**When**: Round completes and more rounds remain

```json
{
  "event": "new_round",
  "gameDuration": 1260000,
  "breakDuration": 60000,
  "currentRound": 2
}
```

---

### Settings Event

#### settings
**When**: Client connects or settings updated

```json
{
  "event": "settings",
  "settings": {
    "gameDuration": 1260000,
    "breakDuration": 60000,
    "numRounds": 3,
    "breakTimerEnabled": true,
    "sirenLength": 1000,
    "sirenPause": 1000
  }
}
```

---

### Schedule Events

#### schedules_list
**When**: Response to `get_schedules` action or after authentication

```json
{
  "event": "schedules_list",
  "schedules": [
    {
      "id": "1698765432-1",
      "clubName": "Badminton Club A",
      "ownerUsername": "operator1",
      "dayOfWeek": 1,
      "startHour": 18,
      "startMinute": 30,
      "durationMinutes": 90,
      "enabled": true,
      "createdAt": 1698765432
    }
  ]
}
```

**Filtering**:
- ADMIN: All schedules
- OPERATOR: Only schedules where `ownerUsername` matches
- VIEWER: All schedules

---

#### schedule_added
**When**: Schedule successfully created

```json
{
  "event": "schedule_added",
  "schedule": {
    "id": "1698765432-1",
    "clubName": "Badminton Club A",
    "ownerUsername": "operator1",
    "dayOfWeek": 1,
    "startHour": 18,
    "startMinute": 30,
    "durationMinutes": 90,
    "enabled": true,
    "createdAt": 1698765432
  }
}
```

**Sent to**: Requester only
**Client Action**: Add schedule to local array, re-render calendar

---

#### schedule_updated
**When**: Schedule successfully updated

```json
{
  "event": "schedule_updated",
  "schedule": {
    "id": "1698765432-1",
    "clubName": "Badminton Club A (Updated)",
    "ownerUsername": "operator1",
    "dayOfWeek": 2,
    "startHour": 19,
    "startMinute": 0,
    "durationMinutes": 120,
    "enabled": true,
    "createdAt": 1698765432
  }
}
```

**Sent to**: Requester only
**Client Action**: Update schedule in local array, re-render calendar

---

#### schedule_deleted
**When**: Schedule successfully deleted

```json
{
  "event": "schedule_deleted",
  "id": "1698765432-1"
}
```

**Sent to**: Requester only
**Client Action**: Remove schedule from local array, re-render calendar

---

#### scheduling_status
**When**: Scheduling system enabled/disabled

```json
{
  "event": "scheduling_status",
  "enabled": true
}
```

**Sent to**: All clients (broadcast)
**Client Action**: Update toggle switch UI

---

### User Management Events

#### operators_list
**When**: Response to `get_operators` action

```json
{
  "event": "operators_list",
  "operators": ["operator1", "operator2"]
}
```

**Sent to**: Requester only (ADMIN)
**Note**: Passwords are never included. Operators are returned as a flat string array.

---

#### operator_added
**When**: Operator account successfully created

```json
{
  "event": "operator_added",
  "username": "newoperator"
}
```

**Sent to**: Requester only (ADMIN)
**Client Action**: Add operator to list, show success message

---

#### operator_removed
**When**: Operator account successfully deleted

```json
{
  "event": "operator_removed",
  "username": "operator1"
}
```

**Sent to**: Requester only (ADMIN)
**Client Action**: Remove operator from list, show success message

---

#### password_changed
**When**: Password successfully changed

```json
{
  "event": "password_changed"
}
```

**Sent to**: Requester only
**Client Action**: Show success notification, clear password fields

---

#### factory_reset_complete
**When**: Factory reset successfully executed

```json
{
  "event": "factory_reset_complete"
}
```

**Sent to**: Requester only (ADMIN)
**Client Action**: Show success message, reload page

---

### Timezone Events

#### timezone_changed
**When**: Timezone successfully updated

```json
{
  "event": "timezone_changed",
  "timezone": "Pacific/Auckland",
  "message": "Timezone updated to Pacific/Auckland"
}
```

**Sent to**: Requester only

---

### NTP Status Event

#### ntp_status
**When**: NTP sync status changes, sent every 5 seconds

```json
{
  "event": "ntp_status",
  "synced": true,
  "time": "10:45:30 AM",
  "timezone": "Pacific/Auckland",
  "dateTime": "2025-10-30 10:45:30",
  "autoSyncInterval": 30
}
```

**When `synced=false`:**
```json
{
  "event": "ntp_status",
  "synced": false,
  "time": "Not synced"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `synced` | boolean | Whether NTP time is synchronized |
| `time` | string | Formatted time string, or "Not synced" |
| `timezone` | string | IANA timezone (only when synced) |
| `dateTime` | string | ISO-style date and time (only when synced) |
| `autoSyncInterval` | number | Sync interval in minutes (only when synced) |

**Sent to**: All clients (broadcast), only on status change
**Validation**: Server checks year (2020-2100), month (1-12), day (1-31)

---

### Hello Club Events

#### upcoming_events
**When**: Response to `get_upcoming_events` action

```json
{
  "event": "upcoming_events",
  "events": [],
  "lastSync": 1698765432,
  "lastError": "",
  "apiConfigured": true
}
```

| Field | Type | Description |
|-------|------|-------------|
| `events` | array | List of upcoming Hello Club events |
| `lastSync` | number | Unix timestamp of last successful sync |
| `lastError` | string | Last error message, empty if none |
| `apiConfigured` | boolean | Whether Hello Club API key is configured |

**Sent to**: Requester only

---

#### helloclub_settings
**When**: Response to `get_helloclub_settings` action

```json
{
  "event": "helloclub_settings",
  "apiKey": "***configured***",
  "enabled": true,
  "defaultDuration": 12
}
```

| Field | Type | Description |
|-------|------|-------------|
| `apiKey` | string | "***configured***" if set, empty string if not |
| `enabled` | boolean | Whether Hello Club integration is enabled |
| `defaultDuration` | number | Default event duration in minutes |

**Sent to**: Requester only (ADMIN)
**Note**: The actual API key is never sent to clients. The field indicates whether a key is configured.

---

#### helloclub_settings_saved
**When**: Hello Club settings successfully saved

```json
{
  "event": "helloclub_settings_saved",
  "message": "Hello Club settings saved"
}
```

**Sent to**: Requester only (ADMIN)

---

#### helloclub_refresh_result
**When**: Response to `helloclub_refresh` action

```json
{
  "event": "helloclub_refresh_result",
  "success": true,
  "message": "Refreshed successfully",
  "eventCount": 3,
  "totalEvents": 10,
  "debug": ""
}
```

| Field | Type | Description |
|-------|------|-------------|
| `success` | boolean | Whether the refresh succeeded |
| `message` | string | Human-readable result message |
| `eventCount` | number | Number of upcoming events returned |
| `totalEvents` | number | Total events from API |
| `debug` | string | Debug information (may be empty) |

**Sent to**: Requester only

---

#### event_auto_started
**When**: Hello Club event automatically triggers the timer

```json
{
  "event": "event_auto_started",
  "eventName": "Tuesday Badminton",
  "durationMin": 90,
  "eventEndTime": 1698800000
}
```

| Field | Type | Description |
|-------|------|-------------|
| `eventName` | string | Name of the Hello Club event |
| `durationMin` | number | Event duration in minutes |
| `eventEndTime` | number | Unix timestamp when the event ends |

**Sent to**: All clients (broadcast)

---

#### event_auto_resumed
**When**: Timer recovered after a mid-event reboot

```json
{
  "event": "event_auto_resumed",
  "eventName": "Tuesday Badminton",
  "durationMin": 60,
  "currentRound": 2,
  "remainingMs": 3600000,
  "eventEndTime": 1698800000
}
```

| Field | Type | Description |
|-------|------|-------------|
| `eventName` | string | Name of the Hello Club event |
| `durationMin` | number | Remaining duration in minutes |
| `currentRound` | number | Round being resumed |
| `remainingMs` | number | Milliseconds remaining |
| `eventEndTime` | number | Unix timestamp when the event ends |

**Sent to**: All clients (broadcast)

---

#### event_cutoff
**When**: Hello Club event end time is reached while timer is running

```json
{
  "event": "event_cutoff",
  "message": "Event ended, timer stopped",
  "eventName": "Tuesday Badminton"
}
```

**Sent to**: All clients (broadcast)
**Behavior**: The timer is automatically stopped when the scheduled event end time is reached.

---

### QR Code Events

#### qr_config
**When**: Response to `get_qr_config` action

```json
{
  "event": "qr_config",
  "ssid": "BadmintonTimer",
  "ssidOverride": "",
  "connectedSsid": "MyWiFi",
  "password": "wifi-password",
  "encryption": "WPA",
  "appUrl": "http://192.168.1.100"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `ssid` | string | Default SSID (from connected network) |
| `ssidOverride` | string | User-configured SSID override, empty if not set |
| `connectedSsid` | string | SSID the ESP32 is currently connected to |
| `password` | string | Configured WiFi password for QR code |
| `encryption` | string | "WPA", "WEP", or "nopass" |
| `appUrl` | string | URL to access the timer web interface |

**Sent to**: Requester only

---

#### qr_settings_saved
**When**: QR code settings successfully saved

```json
{
  "event": "qr_settings_saved",
  "message": "QR settings saved"
}
```

**Sent to**: Requester only (ADMIN)

---

### Error Event

#### error
**When**: Any error occurs

```json
{
  "event": "error",
  "message": "ERR_AUTH_FAILED: Invalid username or password"
}
```

**Error Codes**:
- `ERR_AUTH_FAILED`: Authentication failed
- `ERR_RATE_LIMIT`: Too many requests (>10/second)
- `ERR_PASSWORD_CHANGE`: Old password incorrect
- `ERR_PERMISSION`: Permission denied
- Other errors: Plain text messages

---

## Error Codes and Messages

| Error Message | Cause | Solution |
|--------------|-------|----------|
| "ERR_AUTH_FAILED: Invalid username or password" | Wrong credentials | Check username/password, verify operator account exists |
| "ERR_RATE_LIMIT: Too many requests. Please slow down." | >10 messages/second | Reduce message frequency |
| "ERR_PASSWORD_CHANGE: Old password is incorrect" | Wrong old password in change_password | Verify current password |
| "Permission denied - viewer mode" | Viewer attempting control action | Login as operator or admin |
| "Permission denied - admin only" | Non-admin attempting admin action | Login as admin |
| "Permission denied - you can only edit your own schedules" | Operator editing another operator's schedule | Only admins can edit all schedules |
| "Timer already active. Reset first." | Start attempted while RUNNING/PAUSED | Reset timer first |
| "Schedule not found" | Invalid schedule ID | Refresh schedules list |
| "Invalid day of week (must be 0-6)" | dayOfWeek out of range | Use 0 (Sunday) through 6 (Saturday) |
| "Invalid hour (must be 0-23)" | startHour out of range | Use 0-23 |
| "Invalid minute (must be 0-59)" | startMinute out of range | Use 0-59 |
| "Username already exists" | Duplicate operator username | Choose different username |
| "Password must be at least 4 characters" | Password too short | Use longer password |
| "Maximum number of operators reached" | 10 operators already exist | Remove unused operators first |

---

## Best Practices

### Client-Side

1. **Handle all events** - Don't assume only certain events will arrive
2. **Use `requestAnimationFrame`** - For smooth 60fps timer display
3. **Listen for `sync`** - Update timer baseline every 5 seconds
4. **Handle disconnections** - Implement reconnection with exponential backoff
5. **Validate before sending** - Check inputs before sending actions
6. **Show feedback** - Display errors and success messages
7. **Respect roles** - Hide/disable controls based on user role
8. **Nest schedule data** - Always send schedules under `{ schedule: {...} }` key
9. **Never send `ownerUsername`** - Server sets this automatically
10. **Check NTP status** - Warn users if time not synced before using schedules

### Server-Side

1. **Always validate input** - Check all fields for valid ranges
2. **Check authentication** - Verify client role for all control actions
3. **Check permissions** - Verify ownership for schedule operations
4. **Broadcast state changes** - Send events to ALL clients (not just requester)
5. **Handle errors gracefully** - Send error messages with ERR_ codes
6. **Log important events** - Use DEBUG macros for troubleshooting
7. **Rate limit** - Enforce 10 messages/second per client
8. **Enforce ownership** - Never accept `ownerUsername` from client
9. **Session timeout** - Check inactivity every 60 seconds
10. **Clean up on disconnect** - Remove client from all tracking maps

---

## Example Client Implementation

### Connect and Handle State

```javascript
const socket = new WebSocket('ws://badminton-timer.local/ws');

socket.onmessage = (event) => {
    const data = JSON.parse(event.data);

    switch(data.event) {
        case 'login_prompt':
            showLoginForm();
            break;

        case 'state':
            updateTimerDisplay(data.state);
            break;

        case 'auth_success':
            userRole = data.role;
            currentUsername = data.username;
            hideLoginModal();
            enableControls();
            break;

        case 'viewer_mode':
            userRole = 'viewer';
            disableControls();
            break;

        case 'session_timeout':
            userRole = 'viewer';
            disableControls();
            showNotification('Session expired - Login again or continue as viewer');
            break;

        case 'schedules_list':
            schedules = data.schedules;
            renderCalendar();
            break;

        case 'schedule_added':
            schedules.push(data.schedule);
            renderCalendar();
            showNotification('Schedule added successfully');
            break;

        case 'pause_after_next_changed':
            updatePauseAfterNextUI(data.enabled);
            break;

        case 'upcoming_events':
            renderUpcomingEvents(data.events);
            break;

        case 'event_auto_started':
            showNotification('Event started: ' + data.eventName);
            break;

        case 'ntp_status':
            updateNTPIndicator(data);
            break;

        case 'error':
            showError(data.message);
            break;
    }
};
```

### Authenticate

```javascript
function login(username, password) {
    socket.send(JSON.stringify({
        action: 'authenticate',
        username: username,
        password: password
    }));
}

function continueAsViewer() {
    socket.send(JSON.stringify({
        action: 'authenticate',
        username: '',
        password: ''
    }));
}
```

### Manage Schedules

```javascript
function addSchedule(clubName, dayOfWeek, startHour, startMinute, duration) {
    socket.send(JSON.stringify({
        action: 'add_schedule',
        schedule: {
            clubName: clubName,
            dayOfWeek: dayOfWeek,
            startHour: startHour,
            startMinute: startMinute,
            durationMinutes: duration,
            enabled: true
        }
    }));
}

function updateSchedule(schedule) {
    socket.send(JSON.stringify({
        action: 'update_schedule',
        schedule: schedule  // Must include id
    }));
}

function deleteSchedule(scheduleId) {
    socket.send(JSON.stringify({
        action: 'delete_schedule',
        id: scheduleId
    }));
}
```

### Manage Users (Admin Only)

```javascript
function addOperator(username, password) {
    socket.send(JSON.stringify({
        action: 'add_operator',
        username: username,
        password: password
    }));
}

function removeOperator(username) {
    socket.send(JSON.stringify({
        action: 'remove_operator',
        username: username
    }));
}
```

---

## Troubleshooting

### WebSocket won't connect
- Check ESP32 is powered and connected to WiFi
- Verify correct hostname/IP
- Check firewall settings
- View browser console for errors

### Authentication fails repeatedly
- Verify password in `src/wifi_credentials.h`
- Check firmware was uploaded after password change
- Try hard refresh (Ctrl+Shift+R)
- Check serial monitor for auth logs

### Schedules not triggering
- Check NTP status shows synced (green checkmark)
- Verify scheduling toggle is ON
- Check schedule time matches configured timezone
- Wait 2 minutes after last trigger (cooldown)

### Rate limit errors
- Reduce frequency of WebSocket messages
- Don't send more than 10 messages per second
- Add debouncing to button clicks

### Session timeout too frequent
- Default is 30 minutes of inactivity
- Any message to server resets timeout
- Adjust SESSION_TIMEOUT in main.cpp if needed

---

**API Version**: 3.2.0
**Last Updated**: 2026-03-18
**Project**: ESP32 Badminton Timer
