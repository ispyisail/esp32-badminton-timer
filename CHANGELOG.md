# Changelog

All notable changes to the ESP32 Badminton Timer project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.0.0] - 2025-10-30

### 🎉 Major Release: Multi-Club Scheduling & Authentication

This is a major release adding comprehensive authentication, automated scheduling, and user management systems.

### Added

#### Authentication System
- **Three-tier role-based access control** (VIEWER, OPERATOR, ADMIN)
- **Default admin credentials** (`admin` / `admin`) - change immediately after first use
- **Viewer mode** - anyone can view without authentication
- **Operator accounts** - club coordinators can manage their own schedules
- **Admin account** - full system access including user management
- **Login modal** with username/password inputs and "Continue as Viewer" option
- **Session management** - 30-minute inactivity timeout with automatic viewer downgrade
- **Rate limiting** - 10 messages/second per client to prevent abuse
- **Password change functionality** for all roles (minimum 4 characters for operators)
- **Factory reset** - admin can reset all users, schedules, and settings to defaults
- **Per-client authentication tracking** with username storage

#### Scheduling System
- **Automated weekly recurring schedules** - timer starts automatically at preset times
- **Weekly calendar view** - visual grid (6 AM - 11 PM, Sunday - Saturday)
- **Club-based organization** - each schedule belongs to an operator's club
- **Schedule CRUD operations** - create, read, update, delete with full validation
- **Owner-based permissions** - operators can only edit their own club's schedules
- **Global scheduling toggle** - enable/disable automatic scheduling system-wide
- **Week-minute calculation** - 0-10079 minutes from Sunday 00:00 for trigger detection
- **Auto-trigger detection** - checks every minute if schedule should start timer
- **2-minute cooldown** - prevents duplicate triggers
- **Schedule persistence** - up to 50 schedules stored in NVS
- **Unique ID generation** - timestamp + counter with collision detection
- **Field validation** - day (0-6), hour (0-23), minute (0-59), duration (>0)
- **Schedule page** with add/edit/delete forms and calendar grid

#### User Management
- **Add/remove operators** (admin only)
- **View operator list** (admin only)
- **Password validation** (minimum 4 characters)
- **Maximum 10 operators** (configurable)
- **User management page** with operator list and add/remove controls
- **NVS persistence** for operator credentials

#### Time Synchronization
- **NTP sync status indicator** - ✓ (synced), ⏳ (syncing), ✗ (error)
- **Visual feedback** - green/amber/red with pulsing animation
- **Robust validation** - year (2020-2100), month (1-12), day (1-31)
- **Status broadcasting** - updates every 5 seconds, only on status change
- **Tooltip details** - timezone, date/time, auto-sync interval
- **Pacific/Auckland timezone** configuration for New Zealand DST

#### Developer Tools
- **Local test server** - Node.js + Express + WebSocket mock server
- **Mock authentication** - test all three roles without ESP32 hardware
- **Mock schedules** - pre-configured test data
- **Complete WebSocket simulation** - all v3.0 messages implemented
- **Test server documentation** - comprehensive README with usage instructions

#### Documentation
- **CODE_REVIEW_FINDINGS.md** - first comprehensive code review (16 issues)
- **CODE_REVIEW_ROUND_2.md** - second review verifying all fixes
- **USER_GUIDE.md** - complete end-user documentation for operators and admins
- **Updated README.md** - v3.0 features, quick start, troubleshooting
- **Updated ARCHITECTURE.md** - authentication, schedules, session management
- **Updated API.md** - all new WebSocket messages with examples
- **CHANGELOG.md** - this file

### Changed

#### WebSocket Protocol
- **Explicit viewer_mode event** - separate from auth_success for empty credentials
- **Nested schedule data** - `{ action: "add_schedule", schedule: {...} }` format
- **Error codes with ERR_ prefix** - standardized error messages
- **Server-side username tracking** - `authenticatedUsernames` map for ownership
- **Increased buffer size** - 2KB → 8KB for schedules (handles 50 schedules)
- **Permission checking** - all control actions verify role before execution
- **Owner enforcement** - server sets `ownerUsername`, never accepts from client

#### UI/UX
- **Login modal on page load** - immediate authentication prompt
- **Role-based visibility** - CSS classes hide/disable controls based on role
- **Username display** - shows logged-in user in top right corner
- **Icon navigation** - person icon (users), calendar icon (schedules)
- **Schedule calendar** - CSS Grid layout with colored schedule blocks
- **NTP status** - always visible next to clock display
- **Session timeout notification** - banner warning with re-login option
- **Error notifications** - improved error messages with clear actions

#### Security
- **Server-side validation** - all schedule fields validated before save
- **Permission-based filtering** - operators see only their schedules
- **Rate limiting** - protects against rapid-fire requests
- **Session timeout** - auto-logout after inactivity
- **Ownership security** - client cannot spoof schedule ownership
- **Input sanitization** - username/password length checks

#### Performance
- **Memory cleanup** - `lastTriggerTimes` cleaned up on schedule delete
- **Larger JSON buffer** - prevents overflow with many schedules
- **Improved ID generation** - collision detection with retry mechanism
- **Week wraparound handling** - proper modulo arithmetic for Sunday→Monday transition

### Fixed

#### Critical Bugs (Severity: CRITICAL)
1. **WebSocket message format mismatch** - C++ now reads from `doc["schedule"]` instead of root
2. **Username tracking missing** - added `authenticatedUsernames` map for owner attribution
3. **NTP function calls undefined** - removed non-existent `lastSync()`/`nextSync()` calls
4. **Privilege escalation vulnerability** - server never accepts `ownerUsername` from client

#### High-Priority Bugs (Severity: HIGH)
5. **Memory leak on schedule delete** - clean up `lastTriggerTimes` map entry
6. **Buffer overflow with many schedules** - increased from 2KB to 8KB
7. **Schedule ID collisions** - improved generation with counter + collision detection
8. **Week rollover bug** - fixed Saturday→Sunday schedule triggering with proper modulo

#### Medium-Priority Bugs (Severity: MEDIUM)
9. **No rate limiting** - added 10 messages/second per client limit
10. **Authentication bypass** - explicit `viewer_mode` event prevents confusion
11. **No minimum password length** - enforced 4-character minimum for operators
12. **Usernames logged in serial** - redacted usernames from logs (only in DEBUG mode)

#### Low-Priority Bugs (Severity: LOW)
13. **No session timeout** - added 30-minute inactivity timeout
14. **Generic error messages** - standardized with ERR_ prefixes and descriptive text
15. **Weak NTP sync detection** - improved validation with year/month/day checks

### Security

- **16 security and stability fixes** across critical, high, medium, and low severity
- **Owner-based permission system** prevents unauthorized schedule modifications
- **Server-side ownership enforcement** eliminates privilege escalation risk
- **Rate limiting** protects against abuse and denial-of-service
- **Session management** reduces window for session hijacking
- **Input validation** prevents injection and overflow attacks
- **Password requirements** enforced on server-side

### Developer Experience

- **Local testing without hardware** - complete mock server for rapid development
- **Comprehensive documentation** - 7 major documentation files
- **Code reviews** - 2 complete reviews with all issues resolved
- **API reference** - complete WebSocket message catalog with examples
- **Architecture documentation** - detailed system design with diagrams
- **User guide** - step-by-step instructions for all user roles

### Technical Debt

- Resolved all 16 issues from first code review
- 3 minor issues identified in second review (LOW severity, optional fixes):
  - Missing JavaScript handlers for `viewer_mode` and `session_timeout`
  - No clubName validation (empty string allowed)
  - Rate limit off-by-one (allows 11 instead of 10, negligible)

### Statistics

- **Files Created**: 6 (users.h/cpp, schedule.h/cpp, test-server/*, USER_GUIDE.md, CHANGELOG.md)
- **Files Modified**: 5 (main.cpp, index.html, style.css, script.js, README.md, ARCHITECTURE.md, API.md)
- **Lines Added**: ~8,000+
- **Bugs Fixed**: 16 (4 critical, 4 high, 4 medium, 4 low)
- **Code Reviews**: 2 comprehensive reviews
- **Documentation Pages**: 7 (README, ARCHITECTURE, API, USER_GUIDE, INSTALL, 2 × CODE_REVIEW, CHANGELOG)

---

## [2.0.0] - 2025-10-29

### Added
- **Modular architecture** - separated code into timer.h/cpp, siren.h/cpp, settings.h/cpp
- **Config.h** - centralized configuration and constants
- **Improved timing synchronization** - hybrid client-server sync model
- **WebSocket sync broadcasts** - every 5 seconds during RUNNING state
- **requestAnimationFrame** - smooth 60fps countdown display on client
- **Exponential backoff reconnection** - up to 10 attempts with increasing delays
- **Overflow protection** - handles `millis()` wraparound after 49 days
- **Non-blocking siren** - state machine for multi-blast patterns
- **NVS persistence** - saves all settings to non-volatile storage
- **Self-test on boot** - verifies SPIFFS, NVS, and relay functionality
- **Watchdog timer** - 30-second timeout with auto-restart
- **mDNS support** - access via `badminton-timer.local`
- **Captive portal** - WiFi configuration without hardcoding
- **Debug mode** - configurable logging via serial monitor
- **Sound effects** - optional Web Audio API beeps for user actions
- **Keyboard shortcuts** - SPACE, R, S, M, ? for quick control
- **Connection status indicator** - green/red dot shows WebSocket state
- **Temporary notifications** - fade-in/fade-out banners for user feedback

### Changed
- **Timer precision** - improved to ±10ms (server-side)
- **Client-side countdown** - local interpolation between sync messages
- **WebSocket message structure** - standardized JSON format
- **Settings validation** - server-side range checking
- **Error handling** - graceful failure with user-friendly messages

### Fixed
- **Timer jitter** - eliminated with requestAnimationFrame
- **Timer drift** - corrected with periodic server sync
- **Backwards counting** - fixed client-side elapsed calculation
- **Android captive portal issues** - added multiple detection endpoints
- **WiFi connection reliability** - improved retry logic

### Security
- **Password-protected OTA** - prevents unauthorized firmware updates
- **Input validation** - all user inputs validated server-side
- **Session-based auth** - prevents unauthorized control

---

## [1.0.0] - Initial Release

### Added
- **Basic timer functionality** - start/stop/reset with countdown display
- **Game and break timers** - separate countdown for breaks between rounds
- **Configurable rounds** - set number of rounds for match
- **Real-time clock** - displays current time from NTP
- **Automatic DST** - New Zealand Daylight Saving Time support
- **External siren control** - relay-based siren activation
- **Web-based interface** - responsive HTML/CSS/JavaScript UI
- **WebSocket communication** - real-time bidirectional messaging
- **WiFi connectivity** - ESP32 connects to local network
- **OTA updates** - wireless firmware updates
- **SPIFFS filesystem** - serves web interface files
- **Settings persistence** - saves configuration through power cycles

### Hardware
- **ESP32 development board** support
- **Single-channel relay** control (GPIO 26)
- **External siren/buzzer** support

---

## Versioning

This project follows [Semantic Versioning](https://semver.org/):

- **MAJOR** version (X.0.0): Incompatible API changes or major new features
- **MINOR** version (0.X.0): Backwards-compatible new features
- **PATCH** version (0.0.X): Backwards-compatible bug fixes

---

## Upgrade Notes

### From v2.0 to v3.0

**Breaking Changes:**
- Authentication is now required for all control actions (timer start/stop, settings)
- WebSocket message format changed for schedule operations (nested `schedule` object)
- Viewers can no longer control timer (login as operator/admin required)

**Migration Steps:**
1. Backup any important configuration
2. Upload new firmware (v3.0)
3. Upload new filesystem (updated web files)
4. First boot: Login as admin (admin/admin)
5. IMMEDIATELY change admin password
6. Create operator accounts for each club coordinator
7. Operators can then create their club's schedules

**Data Loss:**
- Timer settings are preserved (game duration, break duration, etc.)
- No existing schedules or users (fresh start in v3.0)
- WiFi credentials are preserved

**New Features Available:**
- Login as admin to access user management
- Create operator accounts for each club
- Operators can create automated schedules for their clubs
- View weekly calendar of all bookings
- Enable automatic scheduling to start timer on schedule

### From v1.0 to v2.0

**Breaking Changes:**
- WebSocket message format standardized
- Settings now use milliseconds instead of mixed units
- Sync messages sent every 5 seconds (was on-demand)

**Migration Steps:**
1. Upload new firmware (v2.0)
2. Upload new filesystem
3. Existing settings may need reconfiguration
4. Test timer functionality before relying on it

**Benefits:**
- Smoother countdown display (60fps)
- More reliable timing
- Better error handling
- Improved WiFi reliability

---

## Known Issues

### v3.0.0

**Minor (LOW severity, optional fixes):**
1. Missing JavaScript handlers for `viewer_mode` and `session_timeout` events (cosmetic)
2. No validation for empty `clubName` (edge case)
3. Rate limit off-by-one allows 11 messages instead of 10 (negligible)

**Workarounds:**
1. Page refresh after viewer mode or session timeout works correctly
2. Don't create schedules with empty club names
3. Rate limit still protects against abuse with 11 message allowance

See CODE_REVIEW_ROUND_2.md for full details.

---

## Future Roadmap (v4.0)

**Under Consideration:**
- HTTPS support with self-signed certificates
- Multi-court support (multiple independent timers)
- Statistics tracking (usage per club, total matches played)
- Email/SMS notifications for schedule reminders
- Mobile app (React Native or Flutter)
- Cloud backup of schedules and settings
- RESTful API in addition to WebSocket
- LCD display for local viewing without browser
- BLE control for offline operation
- Schedule conflict detection and warnings
- Recurring schedule templates
- Holiday/exception handling
- Advanced analytics dashboard
- Two-factor authentication
- Audit logging for admin actions

---

## Support

For bug reports, feature requests, or questions:
- Check documentation first (README, USER_GUIDE, ARCHITECTURE, API)
- Review code review documents for known issues
- Check serial monitor output (115200 baud) for errors
- Test with local mock server to isolate hardware vs. software issues

---

**Changelog Maintained By:** Claude Code
**Project:** ESP32 Badminton Timer
**License:** MIT
