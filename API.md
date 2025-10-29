# ESP32 Badminton Timer - WebSocket API Documentation

## Overview

The ESP32 Badminton Timer uses WebSockets for real-time bidirectional communication between the server (ESP32) and clients (web browsers).

**WebSocket Endpoint**: `ws://badminton-timer.local/ws` or `ws://<ESP32_IP>/ws`

**Protocol**: JSON-based messages

**Authentication**: Password required for all control actions

---

## Connection Flow

```
1. Client connects to WebSocket endpoint
2. Server sends { event: "auth_required" }
3. Client sends { action: "authenticate", password: "..." }
4. Server validates password
5. If valid: Server sends { event: "auth_success" } + initial state
6. If invalid: Server sends { event: "error", message: "Invalid password" }
```

---

## Message Format

All messages are JSON objects with an `event` (server→client) or `action` (client→server) field.

### Client → Server (Actions)

#### authenticate
**Purpose**: Authenticate the WebSocket connection

```json
{
  "action": "authenticate",
  "password": "YourWebPassword456!"
}
```

**Response**: `auth_success` or `error`

---

#### start
**Purpose**: Start the timer from IDLE or FINISHED state

```json
{
  "action": "start"
}
```

**Validation**:
- Timer must be in IDLE or FINISHED state
- Client must be authenticated

**Response**: `start` event broadcast to all clients

**Possible Errors**:
- "Timer already active. Reset first." (if timer is RUNNING or PAUSED)
- "Authentication required" (if not authenticated)

---

#### pause
**Purpose**: Pause timer if RUNNING, or resume if PAUSED

```json
{
  "action": "pause"
}
```

**Behavior**:
- If `timerState == RUNNING` → Pause timer, send `pause` event
- If `timerState == PAUSED` → Resume timer, send `resume` event

**Response**: `pause` or `resume` event

---

#### reset
**Purpose**: Reset timer to IDLE state

```json
{
  "action": "reset"
}
```

**Response**: `reset` event broadcast to all clients

---

#### save_settings
**Purpose**: Update timer settings

```json
{
  "action": "save_settings",
  "settings": {
    "gameDuration": 21,              // minutes (1-120)
    "breakDuration": 60,              // seconds (1-3600)
    "numRounds": 3,                   // rounds (1-20)
    "breakTimerEnabled": true,        // boolean
    "sirenLength": 1000,              // milliseconds (100-10000)
    "sirenPause": 1000                // milliseconds (100-10000)
  }
}
```

**Validation**:
- `gameDuration`: 1-120 minutes
- `breakDuration`: 1-3600 seconds, max 50% of game duration
- `numRounds`: 1-20
- `sirenLength`: 100-10000 ms
- `sirenPause`: 100-10000 ms

**Response**: `settings` event broadcast to all clients

**Possible Errors**:
- "Game duration must be between 1 and 120 minutes"
- "Number of rounds must be between 1 and 20"
- "Break duration must be between 1 and 3600 seconds"
- "Break duration cannot exceed 50% of game duration"
- "Siren length must be between 100 and 10000 ms"
- "Siren pause must be between 100 and 10000 ms"

---

### Server → Client (Events)

#### auth_required
**When**: Client connects to WebSocket

```json
{
  "event": "auth_required",
  "message": "Please enter password to control timer"
}
```

**Action**: Client should prompt user for password and send `authenticate` action

---

#### auth_success
**When**: Successful authentication

```json
{
  "event": "auth_success",
  "message": "Authentication successful"
}
```

**Followed by**: `settings` and `state` or `sync` events

---

#### error
**When**: Any error occurs

```json
{
  "event": "error",
  "message": "Error description"
}
```

**Common Messages**:
- "Authentication required. Please enter password."
- "Invalid password"
- "Invalid JSON format"
- "Timer already active. Reset first."
- Various validation error messages

---

#### settings
**When**: Client connects (after auth) or settings are updated

```json
{
  "event": "settings",
  "settings": {
    "gameDuration": 1260000,          // milliseconds (21 minutes)
    "breakDuration": 60000,            // milliseconds (60 seconds)
    "numRounds": 3,
    "breakTimerEnabled": true,
    "sirenLength": 1000,               // milliseconds
    "sirenPause": 1000                 // milliseconds
  }
}
```

**Purpose**: Inform clients of current settings

**Client Action**: Update UI inputs with these values

---

#### state
**When**: Timer is IDLE, client connects, or state changes

```json
{
  "event": "state",
  "state": {
    "status": "IDLE",                  // "IDLE" | "RUNNING" | "PAUSED" | "FINISHED"
    "mainTimer": 1260000,              // milliseconds remaining
    "breakTimer": 60000,               // milliseconds remaining
    "currentRound": 1,                 // 1-indexed
    "numRounds": 3,
    "time": "10:45:30 AM"             // Current time (NZ timezone)
  }
}
```

**Purpose**: Send full state snapshot

**Client Action**: Update UI to reflect current state

---

#### sync
**When**: Timer is RUNNING or PAUSED, sent every 5 seconds + on client connect

```json
{
  "event": "sync",
  "mainTimerRemaining": 1234567,     // milliseconds remaining on main timer
  "breakTimerRemaining": 45678,      // milliseconds remaining on break timer
  "serverMillis": 98765432,          // Server's millis() timestamp
  "currentRound": 2,                 // Current round (1-indexed)
  "numRounds": 3,                    // Total rounds
  "status": "RUNNING"                // "RUNNING" | "PAUSED"
}
```

**Purpose**: Keep clients synchronized with server time

**Client Action**:
- Store `mainTimerRemaining`, `breakTimerRemaining`
- Store `lastSyncTime = Date.now()`
- Calculate current time: `serverTimerRemaining - (Date.now() - lastSyncTime)`
- Update display using requestAnimationFrame

**Critical**: Use `mainTimerRemaining` and `breakTimerRemaining`, NOT `gameDuration`/`breakDuration`

---

#### start
**When**: Timer starts from IDLE or FINISHED

```json
{
  "event": "start",
  "gameDuration": 1260000,           // Total game duration (milliseconds)
  "breakDuration": 60000,            // Total break duration (milliseconds)
  "numRounds": 3,
  "currentRound": 1
}
```

**Purpose**: Notify all clients that timer has started

**Client Action**:
- Set `serverMainTimerRemaining = gameDuration`
- Set `serverBreakTimerRemaining = breakDuration`
- Set `lastSyncTime = Date.now()`
- Start requestAnimationFrame loop

---

#### new_round
**When**: Previous round completes and there are more rounds to play

```json
{
  "event": "new_round",
  "gameDuration": 1260000,           // Reset to full duration
  "breakDuration": 60000,            // Reset to full duration
  "currentRound": 2                  // New round number
}
```

**Purpose**: Signal start of new round

**Client Action**:
- Reset timers to full duration
- Update round counter
- Continue requestAnimationFrame loop

---

#### pause
**When**: Timer is paused (RUNNING → PAUSED)

```json
{
  "event": "pause"
}
```

**Purpose**: Notify all clients that timer is paused

**Client Action**:
- Set `isClientTimerPaused = true`
- Stop requestAnimationFrame loop
- Keep current display values

---

#### resume
**When**: Timer is resumed (PAUSED → RUNNING)

```json
{
  "event": "resume"
}
```

**Purpose**: Notify all clients that timer is resumed

**Client Action**:
- Set `isClientTimerPaused = false`
- Restart requestAnimationFrame loop

---

#### reset
**When**: Timer is reset to IDLE

```json
{
  "event": "reset"
}
```

**Purpose**: Notify all clients that timer was reset

**Client Action**:
- Stop requestAnimationFrame loop
- Reset display to 00:00
- Reset round counter
- Set status to IDLE

---

#### finished
**When**: All rounds complete (RUNNING → FINISHED)

```json
{
  "event": "finished"
}
```

**Purpose**: Notify all clients that match is complete

**Client Action**:
- Stop requestAnimationFrame loop
- Reset display to 00:00
- Show "Match Complete" or celebration animation
- Set status to FINISHED

**Server Behavior**: Triggers 3-blast siren sequence

---

## Error Codes

| Error Message | Cause | Solution |
|--------------|-------|----------|
| "Authentication required" | Action attempted without auth | Send `authenticate` action first |
| "Invalid password" | Wrong password provided | Check `WEB_PASSWORD` in credentials file |
| "Invalid JSON format" | Malformed JSON message | Validate JSON before sending |
| "Timer already active" | Start attempted while RUNNING/PAUSED | Reset timer first |
| "Game duration must be..." | Invalid game duration | Use value between 1-120 minutes |
| "Break duration must be..." | Invalid break duration | Use value between 1-3600 seconds |
| "Number of rounds must be..." | Invalid round count | Use value between 1-20 |

---

## Example Client Implementation

### Connect and Authenticate

```javascript
const socket = new WebSocket('ws://badminton-timer.local/ws');

socket.onmessage = (event) => {
    const data = JSON.parse(event.data);

    if (data.event === 'auth_required') {
        const password = prompt("Enter password:");
        socket.send(JSON.stringify({
            action: 'authenticate',
            password: password
        }));
    }

    if (data.event === 'auth_success') {
        console.log("Authenticated!");
        // Wait for settings and state events
    }
};
```

### Start Timer

```javascript
function startTimer() {
    socket.send(JSON.stringify({ action: 'start' }));
}
```

### Handle Timer Sync

```javascript
let serverMainTimerRemaining = 0;
let serverBreakTimerRemaining = 0;
let lastSyncTime = 0;

socket.onmessage = (event) => {
    const data = JSON.parse(event.data);

    if (data.event === 'sync') {
        serverMainTimerRemaining = data.mainTimerRemaining;
        serverBreakTimerRemaining = data.breakTimerRemaining;
        lastSyncTime = Date.now();
        startAnimationLoop();
    }
};

function clientTimerLoop() {
    const localElapsed = Date.now() - lastSyncTime;
    const currentMainTimer = Math.max(0, serverMainTimerRemaining - localElapsed);
    const currentBreakTimer = Math.max(0, serverBreakTimerRemaining - localElapsed);

    // Update display
    document.getElementById('main-timer').textContent = formatTime(currentMainTimer);
    document.getElementById('break-timer').textContent = formatTime(currentBreakTimer);

    if (currentMainTimer > 0 || currentBreakTimer > 0) {
        requestAnimationFrame(clientTimerLoop);
    }
}
```

### Update Settings

```javascript
function saveSettings() {
    socket.send(JSON.stringify({
        action: 'save_settings',
        settings: {
            gameDuration: 21,              // minutes
            breakDuration: 60,              // seconds
            numRounds: 3,
            breakTimerEnabled: true,
            sirenLength: 1000,
            sirenPause: 1000
        }
    }));
}
```

---

## Best Practices

### Client-Side

1. **Always handle `auth_required`** - Connection requires authentication
2. **Use `requestAnimationFrame`** - For smooth 60fps timer display
3. **Sync regularly** - Listen for `sync` events every 5 seconds
4. **Handle disconnections** - Implement reconnection with exponential backoff
5. **Validate before sending** - Check inputs before sending settings
6. **Show feedback** - Display errors and success messages to users
7. **Disable buttons** - Prevent rapid-fire clicks during processing

### Server-Side

1. **Always validate input** - Check all settings for valid ranges
2. **Check authentication** - Verify client is authenticated for control actions
3. **Broadcast state changes** - Send events to ALL clients (not just requester)
4. **Handle errors gracefully** - Send error messages instead of crashing
5. **Log important events** - Use DEBUG macros for troubleshooting

---

## Troubleshooting

### WebSocket won't connect

- Check ESP32 is powered on and connected to WiFi
- Verify correct hostname/IP address
- Check firewall settings
- View browser console for error messages

### Authentication fails repeatedly

- Verify password in `src/wifi_credentials.h`
- Check firmware was uploaded after setting password
- Try hard refresh (Ctrl+Shift+R)

### Timer display is jittery

- Ensure using `requestAnimationFrame`, not `setInterval`
- Check `sync` events are being received
- Verify network latency is reasonable (<100ms)

### Changes not reflected

- Make sure to upload firmware AND filesystem
- Clear browser cache
- Check browser console for errors

---

**API Version**: 2.0.0
**Last Updated**: 2025-10-29
**Author**: Claude Code
**Project**: ESP32 Badminton Timer
