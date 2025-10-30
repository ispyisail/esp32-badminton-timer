# Code Review Findings

**Date:** 2025-10-30
**Reviewer:** Claude (AI Code Assistant)
**Scope:** Authentication system, Schedule management, NTP sync indicator, WebSocket handlers

---

## CRITICAL Issues (Must Fix Immediately)

### 1. **WebSocket Message Format Mismatch - Schedule Operations Broken**
**Location:** `data/script.js:632-641` vs `src/main.cpp:753-788`

**Issue:** JavaScript sends schedule data nested under `schedule` key, but C++ reads from root level.

**JavaScript sends:**
```javascript
{
    action: 'add_schedule',
    schedule: {
        clubName: "Monday Club",
        dayOfWeek: 1,
        ...
    }
}
```

**C++ expects:**
```cpp
newSchedule.clubName = doc["clubName"] | "";  // This reads from root, not doc["schedule"]["clubName"]
```

**Impact:** ALL schedule add/update operations will fail silently. Schedule data will be empty/default values.

**Fix:** Either change JavaScript to send data at root level, OR change C++ to read from `doc["schedule"]`.

---

### 2. **Username Tracking Not Implemented - Permission System Broken**
**Location:** `src/main.cpp:754, 793`

**Issue 1 - Line 754:**
```cpp
newSchedule.ownerUsername = (clientRole == ADMIN) ? "admin" :
    authenticatedClients[client->id()] == OPERATOR ? "operator" : "";
```
- Sets `ownerUsername` to literal string "operator" for ALL operators
- Doesn't use the actual username from authentication
- No username-to-client-ID mapping exists

**Issue 2 - Line 793:**
```cpp
if (scheduleManager.hasPermission(existingSchedule, "", clientRole == ADMIN)) {
```
- Passes empty string `""` for username
- Operators can NEVER edit their own schedules because username is always ""

**Impact:**
- All operators appear to own all schedules
- Permission checks always fail for operators
- Operators cannot edit their own schedules despite the UI showing they can

**Fix:** Add `std::map<uint32_t, String> authenticatedUsernames;` to track username per client ID.

---

### 3. **Privilege Escalation via ownerUsername Modification**
**Location:** `src/main.cpp:782`

**Issue:**
```cpp
updatedSchedule.ownerUsername = doc["ownerUsername"] | "";
```
- Client can send any `ownerUsername` in update request
- No validation that client owns the schedule
- Operator can change `ownerUsername` to bypass permission checks

**Example Attack:**
1. Operator creates schedule (ownerUsername = "operator1")
2. Operator sends update with ownerUsername = "admin"
3. Now only admin can edit it, or operator can impersonate admin

**Impact:** Complete bypass of multi-tenancy system. Operators can lock out other operators or admins.

**Fix:** Never accept `ownerUsername` from client. Always use existing schedule's owner for updates.

---

### 4. **NTP Functions Not Defined - Compilation Will Fail**
**Location:** `src/main.cpp:450-451`

**Issue:**
```cpp
doc["lastSync"] = lastSync();  // Function not defined
doc["nextSync"] = nextSync();  // Function not defined
```

**Impact:** Code will not compile. NTP status feature is completely broken.

**Fix:** Check if ezTime provides these functions. If not, remove or implement alternatives.

---

## HIGH Severity Issues

### 5. **No Input Validation on Schedule Fields**
**Location:** `src/main.cpp:778-788`

**Issue:** `update_schedule` handler doesn't validate fields like `add_schedule` does.
- No check for `dayOfWeek` range (0-6)
- No check for `startHour` range (0-23)
- No check for `startMinute` range (0-59)
- No check for `durationMinutes` range (1-180)
- `clubName` can be empty or extremely long

**Impact:**
- Invalid data can be saved to NVS
- Could cause schedule trigger logic to fail
- Could cause buffer overflows if clubName is very long

**Fix:** Add validation in `update_schedule` handler matching `add_schedule` validation.

---

### 6. **Memory Leak in Schedule Trigger Tracking**
**Location:** `src/schedule.cpp:226`, `src/schedule.h` (lastTriggerTimes map)

**Issue:** `lastTriggerTimes` map grows indefinitely:
```cpp
void ScheduleManager::markTriggered(const String& id, unsigned long triggerTime) {
    lastTriggerTimes[id] = triggerTime;  // Never cleaned up
}
```

**Impact:**
- If schedules are frequently created/deleted, map grows unbounded
- On ESP32 with limited RAM, could cause memory exhaustion
- Each entry: ~32 bytes (String + unsigned long)
- After 1000 schedule creates: ~32KB wasted

**Fix:** Add cleanup in `deleteSchedule()` to remove entries from `lastTriggerTimes`.

---

### 7. **Schedule ID Collision Risk**
**Location:** `src/schedule.cpp:246-251`

**Issue:**
```cpp
String ScheduleManager::generateScheduleId() {
    unsigned long timestamp = millis();
    int randomNum = random(1000, 9999);
    return String(timestamp) + String(randomNum);
}
```

**Collision scenarios:**
- Two schedules created within same millisecond → same timestamp
- Only 9000 possible random numbers → birthday paradox applies
- Collision probability ≈ 1% after 100 schedules in same millisecond

**Impact:** Duplicate IDs cause schedules to overwrite each other.

**Fix:** Add collision check or use better ID generation (e.g., UUID, or timestamp + counter).

---

### 8. **Buffer Overflow Risk in Schedule List**
**Location:** `src/main.cpp:730`

**Issue:**
```cpp
StaticJsonDocument<2048> schedulesDoc;  // Only 2KB
```

**Calculation:**
- 50 schedules (MAX_SCHEDULES)
- Each schedule JSON: ~150 bytes (clubName + fields)
- Total: 50 × 150 = 7500 bytes
- Buffer: 2048 bytes → **Overflow by 5452 bytes!**

**Impact:**
- Memory corruption if many schedules exist
- ESP32 crash/reboot
- Data loss

**Fix:** Use `DynamicJsonDocument` or increase buffer to 8192+ bytes.

---

## MEDIUM Severity Issues

### 9. **Week Minute Rollover Bug in Trigger Cooldown**
**Location:** `src/schedule.cpp:211`

**Issue:**
```cpp
if ((currentWeekMinute - lastTrigger) < 2) {
    continue; // Don't re-trigger
}
```

**Problem:** At week boundary (Saturday 23:59 → Sunday 00:00):
- `lastTrigger` = 10079 (Saturday 23:59)
- `currentWeekMinute` = 0 (Sunday 00:00)
- Subtraction: 0 - 10079 = -10079 (unsigned wraps to huge number)
- Check fails, allowing duplicate trigger

**Impact:** Schedules at Saturday night will re-trigger on Sunday morning.

**Fix:** Use absolute time comparison or modulo arithmetic for week wraparound.

---

### 10. **No Rate Limiting on WebSocket Messages**
**Location:** `src/main.cpp:484` (handleWebSocketMessage)

**Issue:** No throttling or rate limiting on incoming WebSocket messages.

**Attack scenario:**
- Malicious client sends 1000 messages/second
- Each message allocates JSON buffer, processes request
- ESP32 becomes unresponsive
- Legitimate users cannot control timer

**Impact:** Denial of Service (DoS) vulnerability.

**Fix:** Add per-client rate limiting (e.g., max 10 messages/second).

---

### 11. **Authentication Bypass for Empty Credentials**
**Location:** `src/main.cpp:509`

**Issue:**
```cpp
if (role != VIEWER || (username.isEmpty() && password.isEmpty())) {
    authenticatedClients[client->id()] = role;  // Sets to VIEWER
    authDoc["event"] = "auth_success";
```

**Problem:** Empty username/password returns VIEWER role but still sends "auth_success".

**Impact:**
- Confusing UX: empty credentials show "success"
- Could mask authentication failures
- Not technically a security issue but poor design

**Fix:** Handle viewer mode explicitly, don't treat failed auth as success.

---

### 12. **No Minimum Password Length**
**Location:** `src/users.cpp:113` (addOperator), `160` (changePassword)

**Issue:** Passwords can be 1 character or any length.

**Impact:** Weak passwords reduce security (though this is a local network device).

**Fix:** Add minimum length check (e.g., 4+ characters).

---

## LOW Severity Issues

### 13. **Serial Logging of Authentication Attempts**
**Location:** `src/users.cpp:96, 103, 109`

**Issue:** Logs successful/failed authentication with usernames.

**Impact:** If serial port is accessible, attacker can see valid usernames and monitor auth attempts.

**Fix:** Remove username from logs or add compile-time flag to disable.

---

### 14. **No Session Timeout**
**Location:** WebSocket authentication system

**Issue:** Once authenticated, client stays authenticated until WebSocket disconnects.

**Impact:**
- Shared devices: previous user stays logged in
- Lost/stolen tablets remain authenticated
- No automatic logout

**Fix:** Add session timeout (e.g., 30 minutes of inactivity).

---

### 15. **Inconsistent Error Messages**
**Location:** Various `sendError()` calls

**Issue:** Some errors are generic ("An error occurred"), others are specific.

**Impact:** Difficult to debug issues for users.

**Fix:** Standardize error messages with error codes.

---

### 16. **NTP Sync Detection Heuristic**
**Location:** `src/main.cpp:444, 476`

**Issue:**
```cpp
bool synced = myTZ.year() > 2020;
```

**Problem:** If system clock is manually set to 2021+ (even if wrong), reports as "synced".

**Impact:** False positive sync status if time is incorrectly set.

**Fix:** Use proper ezTime sync status function if available (e.g., `timeStatus()`).

---

## Summary

| Severity | Count | Must Fix Before Production |
|----------|-------|---------------------------|
| **CRITICAL** | 4 | ✅ YES |
| **HIGH** | 4 | ✅ YES |
| **MEDIUM** | 4 | ⚠️ Recommended |
| **LOW** | 4 | 💡 Optional |

**Total Issues:** 16

---

## Recommended Fix Priority

1. **Fix #1** - WebSocket message format mismatch (schedules completely broken)
2. **Fix #2** - Username tracking (permission system broken)
3. **Fix #4** - NTP compilation error (code won't compile)
4. **Fix #3** - Privilege escalation (security)
5. **Fix #5** - Input validation (data integrity)
6. **Fix #8** - Buffer overflow (crash risk)
7. **Fix #6** - Memory leak (long-term stability)
8. **Fix #7** - ID collisions (data integrity)
9. **Remaining issues** - As time permits

---

## Positive Findings

Despite these issues, the code demonstrates:
- ✅ Good separation of concerns (modular design)
- ✅ Consistent coding style
- ✅ Use of const correctness
- ✅ Proper use of vector instead of arrays
- ✅ Non-blocking timer implementation
- ✅ WebSocket-based real-time updates
- ✅ Comprehensive UI with role-based access

The architecture is solid; these are implementation details that need correction.
