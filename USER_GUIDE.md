# ESP32 Badminton Timer - User Guide

**Version**: 3.0.0
**Last Updated**: 2025-10-30

This guide is for **operators** and **admins** who will be using the badminton timer system on a daily basis. If you're looking for installation instructions, see INSTALL_GUIDE.md instead.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [User Roles](#user-roles)
3. [Logging In](#logging-in)
4. [Using the Timer](#using-the-timer)
5. [Managing Schedules](#managing-schedules)
6. [User Management (Admin Only)](#user-management-admin-only)
7. [Understanding NTP Time Sync](#understanding-ntp-time-sync)
8. [Common Tasks](#common-tasks)
9. [Troubleshooting](#troubleshooting)
10. [Tips and Best Practices](#tips-and-best-practices)

---

## Getting Started

### Accessing the Timer

1. Make sure your device (phone, tablet, or computer) is connected to the same WiFi network as the ESP32 timer
2. Open a web browser
3. Navigate to: **http://badminton-timer.local**
4. If that doesn't work, use the IP address (ask your network administrator)

### First Time Setup (Admin Only)

When you first access the timer:

1. You'll see a login screen
2. Login with the default admin credentials:
   - **Username**: `admin`
   - **Password**: `admin`
3. **IMMEDIATELY change the admin password:**
   - Click the person icon (top right)
   - Select "Change Password"
   - Enter current password (`admin`)
   - Enter and confirm new password
   - Click "Change Password"
4. Create operator accounts for each club coordinator

---

## User Roles

The system has three levels of access:

### 🔵 Viewer (No Login Required)

**What you can do:**
- View the timer countdown
- See the current time
- View the schedule calendar
- No controls available (read-only)

**Best for:**
- Players waiting for their court time
- Spectators
- Anyone who just needs to see the timer

**How to access:**
- Open the webpage
- Click "Continue as Viewer" on the login screen
- OR just close the login modal

---

### 🟢 Operator (Club Coordinator)

**What you can do:**
- Everything a Viewer can do, plus:
- Start/stop the timer manually
- Create schedules for your club
- Edit and delete your own club's schedules
- Change your own password
- Enable/disable automatic scheduling

**Cannot do:**
- View or edit other clubs' schedules
- Add/remove operator accounts
- Perform factory reset

**Best for:**
- Club coordinators
- Regular court managers
- Anyone responsible for their club's booking schedule

**How to get access:**
- Ask the system admin to create an operator account for you
- Login with your assigned username and password

---

### 🔴 Admin (System Administrator)

**What you can do:**
- Everything an Operator can do, plus:
- View ALL schedules from ALL clubs
- Add new operator accounts
- Remove operator accounts
- Change admin password
- Perform factory reset

**Responsibilities:**
- Create operator accounts for each club
- Resolve scheduling conflicts
- Monitor overall system usage
- Perform factory reset if needed
- Keep admin password secure

**Best for:**
- Facility managers
- IT administrators
- Primary system owners

---

## Logging In

### Option 1: Login with Credentials

1. When you first open the webpage, a login modal appears
2. Enter your username (e.g., "admin" or your operator username)
3. Enter your password
4. Click "Login"
5. If successful, you'll see your username in the top right corner

### Option 2: Continue as Viewer

1. Click "Continue as Viewer" button on the login modal
2. You'll have read-only access
3. All control buttons will be disabled (grayed out)

### Logged In Indicators

When logged in, you'll see:
- Your username displayed at the top right
- Role badge (Admin/Operator/Viewer)
- Enabled control buttons (if operator/admin)
- Access to schedule management (calendar icon)
- Access to user management (person icon, admin only)

---

## Using the Timer

### Starting the Timer Manually

1. Make sure the timer is in IDLE state (showing 00:00)
2. Click the green **"START"** button
3. The timer will begin counting down
4. A siren will sound at the end (2 blasts)

**Note:** Viewers cannot start the timer - only operators and admins.

### Pausing the Timer

1. While timer is running, click the yellow **"PAUSE"** button
2. Timer will freeze at current time
3. Click "PAUSE" again to resume

### Stopping the Timer

1. Click the red **"RESET"** button at any time
2. Timer returns to IDLE state (00:00)
3. You can now start a new session

### Reading the Display

The timer shows:
- **Main Timer**: Large countdown display (game time)
- **Current Time**: Clock display at top (synced via internet)
- **NTP Status**: Indicator next to clock:
  - ✓ (green) = Time synced, schedules will work
  - ⏳ (amber) = Syncing time, please wait
  - ✗ (red) = Time not synced, schedules may not work

---

## Managing Schedules

Schedules allow the timer to start automatically at preset times every week.

### Accessing the Schedule Page

1. Login as an operator or admin
2. Click the **calendar icon** in the top navigation bar
3. You'll see the weekly schedule calendar

### Understanding the Schedule Calendar

- **Columns**: Days of the week (Sunday through Saturday)
- **Rows**: Hours of the day (6 AM to 11 PM)
- **Colored blocks**: Existing schedules
- **Click any block**: Edit or delete that schedule

### Creating a New Schedule

1. Click the **"Add Schedule"** button (above the calendar)
2. Fill in the form:
   - **Club Name**: Your club's name (e.g., "Eagles Badminton Club")
   - **Day of Week**: Select from Sunday through Saturday
   - **Start Hour**: Hour in 24-hour format (18 = 6 PM)
   - **Start Minute**: Minute (0-59)
   - **Duration**: How many minutes the session lasts (e.g., 90 for 1.5 hours)
   - **Enabled**: Check this box to activate the schedule
3. Click **"Save Schedule"**
4. Your schedule appears on the calendar

**Example:**
- Club Name: "Morning Stars"
- Day: Wednesday
- Start Hour: 18 (6 PM)
- Start Minute: 30
- Duration: 90 minutes
- Result: Timer starts every Wednesday at 6:30 PM for 90 minutes

### Editing a Schedule

1. Click the colored schedule block on the calendar
2. Form populates with existing values
3. Make your changes
4. Click **"Save Schedule"**

### Deleting a Schedule

1. Click the colored schedule block on the calendar
2. Click the **"Delete Schedule"** button
3. Confirm deletion
4. Schedule is removed from calendar and NVS storage

### Enabling Automatic Scheduling

For schedules to actually trigger the timer:

1. Make sure the **"Automatic Scheduling" toggle** (at top of schedule page) is **ON** (green)
2. If it's off, schedules are stored but won't trigger the timer
3. Only operators and admins can toggle this setting

### How Auto-Start Works

When automatic scheduling is enabled:

1. ESP32 checks every minute if any schedule should start
2. If current time matches a schedule's start time, timer starts automatically
3. Timer runs for the scheduled duration
4. A 2-minute cooldown prevents re-triggering
5. **Important**: NTP time must be synced (green ✓) for schedules to work

### Viewing Schedules (Role-Based)

- **Operators**: See only schedules they created (where owner matches their username)
- **Admins**: See ALL schedules from ALL clubs
- **Viewers**: See all schedules (read-only)

This ensures operators can't accidentally modify another club's booking times.

---

## User Management (Admin Only)

### Viewing Operators

1. Login as admin
2. Click the **person icon** (top right)
3. Select **"Manage Users"**
4. You'll see a list of all operator accounts

### Adding a New Operator

1. Go to "Manage Users" page
2. Enter the new operator's username (e.g., "club_eagles")
3. Enter a password (minimum 4 characters)
4. Click **"Add Operator"**
5. Give the username and password to the club coordinator

**Tips:**
- Use descriptive usernames like "club_name" or "coordinator_name"
- Passwords must be at least 4 characters
- Maximum 10 operators allowed
- Write down credentials and give them to the operator securely

### Removing an Operator

1. Go to "Manage Users" page
2. Find the operator in the list
3. Click **"Remove"** next to their username
4. Confirm deletion

**Important:** Removing an operator does NOT delete their schedules. The schedules remain with the original owner's username.

### Changing Your Password

**For Operators:**
1. Click the person icon (top right)
2. Select "Change Password"
3. Enter your current password
4. Enter new password (minimum 4 characters)
5. Confirm new password
6. Click "Change Password"

**For Admin:**
1. Same process as above
2. Old password must be correct
3. New password has no minimum length (but use a strong one!)

### Factory Reset (Admin Only)

**⚠️ WARNING: This cannot be undone!**

A factory reset will:
- Delete ALL operator accounts
- Delete ALL schedules
- Reset admin password to "admin"
- Disable automatic scheduling
- Reset all timer settings to defaults

**To perform factory reset:**
1. Login as admin
2. Click person icon → Settings
3. Click "Factory Reset" button
4. Confirm you understand this is irreversible
5. System resets immediately

**When to use:**
- Starting fresh with new clubs/operators
- Admin password forgotten (requires ESP32 physical access to re-upload firmware)
- System has corrupted data

---

## Understanding NTP Time Sync

### What is NTP?

NTP (Network Time Protocol) automatically synchronizes the ESP32's clock with internet time servers. This ensures accurate timekeeping for schedules.

### NTP Status Indicators

Next to the clock display, you'll see one of three indicators:

**✓ (Green Check mark)**
- Time is successfully synced
- Schedules will trigger at correct times
- Everything is working normally

**⏳ (Amber Hourglass, pulsing)**
- Currently syncing with time server
- Wait 30-60 seconds after ESP32 boots
- Schedules may not work until syncing completes

**✗ (Red X, pulsing)**
- Time sync failed
- ESP32 may not have internet access
- **Schedules will NOT work correctly**
- Check network connection

### Why NTP Matters

- Schedules depend on accurate time
- Without NTP sync, the ESP32's clock may drift
- Drifting clock causes schedules to trigger at wrong times
- Always check for green ✓ before relying on automatic scheduling

### Troubleshooting NTP

If you see ✗ (red X):

1. Check ESP32 has internet access (can it reach external websites?)
2. Verify router allows UDP port 123 (NTP port)
3. Wait 2-3 minutes after power-on
4. Check serial monitor for NTP error messages
5. Try restarting ESP32

---

## Common Tasks

### Task: Setting Up Weekly Court Times

**Scenario:** Your club has court time every Tuesday and Thursday from 7:00 PM to 9:00 PM.

1. Login as operator
2. Go to Schedules (calendar icon)
3. Turn on "Automatic Scheduling" toggle
4. Click "Add Schedule"
5. Create first schedule:
   - Club Name: Your club name
   - Day: Tuesday
   - Hour: 19 (7 PM in 24-hour format)
   - Minute: 0
   - Duration: 120 (2 hours)
   - Enabled: ✓
6. Click "Save"
7. Click "Add Schedule" again
8. Create second schedule:
   - Club Name: Your club name
   - Day: Thursday
   - Hour: 19
   - Minute: 0
   - Duration: 120
   - Enabled: ✓
9. Click "Save"

Result: Timer will automatically start every Tuesday and Thursday at 7:00 PM for 2 hours.

---

### Task: Temporarily Disabling Your Club's Schedule

**Scenario:** Your club is on holiday next week and won't use the court.

**Option 1: Disable Individual Schedules**
1. Click each of your schedule blocks
2. Uncheck "Enabled"
3. Click "Save"
4. Remember to re-enable them later!

**Option 2: Turn Off Automatic Scheduling (affects all clubs)**
1. Toggle "Automatic Scheduling" to OFF
2. No schedules will trigger for any club
3. **Caution:** This affects ALL clubs, not just yours

---

### Task: Resolving Schedule Conflicts (Admin)

**Scenario:** Two clubs accidentally scheduled the same court time.

1. Login as admin
2. Go to Schedules
3. You can see ALL schedules (unlike operators)
4. Identify the conflicting schedules
5. Contact the relevant club coordinators
6. Either:
   - Delete one schedule
   - Edit one schedule to different time
   - Work out sharing arrangement

---

### Task: Giving a New Club Coordinator Access

**Scenario:** A new person is taking over as club coordinator.

1. Login as admin
2. Go to Manage Users (person icon)
3. Click "Add Operator"
4. Username: club_newclub (or coordinator's name)
5. Password: Create a secure password (e.g., "SecurePass123")
6. Click "Add Operator"
7. Contact the new coordinator:
   - Give them the username and password
   - Explain they should change password after first login
   - Show them how to access Schedules page
   - Tell them they can only edit their own club's schedules

---

## Troubleshooting

### Problem: Can't Login

**Symptoms:** "Invalid username or password" error

**Solutions:**
1. Check username is correct (case-sensitive)
2. Check password is correct (case-sensitive)
3. For operators: Verify account was created by admin
4. For admin: Verify you're using correct admin password
5. If admin password is forgotten, see "Hardware Factory Reset" below
6. Check serial monitor for authentication logs

---

### Hardware Factory Reset (Emergency Recovery)

**⚠️ WARNING: This will delete ALL data including:**
- All user accounts (admin and operators)
- All schedules
- All settings
- Hello Club integration settings

**When to use:**
- Admin password is forgotten and cannot be recovered
- System is unresponsive or corrupted
- Need to completely start over

**How to perform hardware factory reset:**

1. **Locate the BOOT button** on your ESP32 board
   - Usually labeled "BOOT", "IO0", or "FLASH"
   - Typically located near the USB port
   - Often colored blue or black

2. **Press and HOLD the BOOT button for 10 seconds**
   - Don't release the button early
   - You'll hear/see feedback every 2 seconds:
     - Serial monitor shows countdown: "Factory reset: 2 seconds...", "Factory reset: 4 seconds...", etc.
     - Relay clicks briefly (if siren connected, you'll hear short beeps)

3. **After 10 seconds**, factory reset will trigger:
   - Serial monitor shows: "FACTORY RESET TRIGGERED!"
   - Relay/siren will beep 5 times rapidly (1 second total)
   - Device displays: "Factory reset complete. Restarting in 3 seconds..."
   - ESP32 automatically restarts

4. **After restart:**
   - All settings are back to factory defaults
   - Admin password is reset to: `admin` / `admin`
   - Timer defaults: 21 minutes, 60 second break, 3 rounds
   - All schedules deleted
   - All operator accounts removed

5. **Immediately change the admin password** after factory reset

**Tips:**
- Hold the button steady for the full 10 seconds
- If you release before 10 seconds, the reset is cancelled (no data loss)
- Watch the serial monitor if possible to see progress
- Make sure the device has power during the entire process

**Can't find the BOOT button?**
- Check your ESP32 board documentation
- Common ESP32 boards (NodeMCU, DevKit, WROOM) all have a BOOT button
- It's usually next to the "EN" (Enable/Reset) button
- GPIO 0 is the BOOT button pin

---

### Problem: Schedule Not Triggering

**Symptoms:** Timer doesn't start at scheduled time

**Check these:**
1. **NTP Status**: Must show green ✓ (not ⏳ or ✗)
2. **Scheduling Toggle**: Must be ON (green)
3. **Schedule Enabled**: Individual schedule must have "Enabled" checked
4. **Correct Time**: Schedule time matches Pacific/Auckland timezone
5. **Cooldown**: Wait 2 minutes after last trigger
6. **Day of Week**: Verify day is correct (0=Sunday, 6=Saturday)

**Steps:**
1. Check NTP indicator - if red ✗, see "Understanding NTP Time Sync"
2. Check "Automatic Scheduling" toggle at top of schedule page
3. Click your schedule block and verify "Enabled" is checked
4. Verify start time is correct (24-hour format)
5. Check serial monitor for schedule trigger logs

---

### Problem: Session Keeps Timing Out

**Symptoms:** After 30 minutes, controls become disabled, "Session expired" message

**Explanation:** This is a security feature. After 30 minutes of inactivity, you're automatically downgraded to viewer mode.

**Solutions:**
1. Send any WebSocket message to reset timer (e.g., refresh page, click anything)
2. Login again when timeout occurs
3. If timeout is too aggressive for your use case, ask developer to adjust `SESSION_TIMEOUT` in main.cpp

---

### Problem: Can't See Other Club's Schedules

**Symptoms:** Calendar only shows your own schedules

**Explanation:** This is by design. Operators only see their own club's schedules.

**Solutions:**
- If you need to see all schedules, login as admin
- If you're admin but only seeing your schedules, check you're logged in as admin (not operator)
- Check username displayed in top right corner

---

### Problem: Calendar Doesn't Update After Adding Schedule

**Symptoms:** Schedule saved but doesn't appear on calendar

**Solutions:**
1. Refresh the page (F5 or Ctrl+R)
2. Check for error message (red notification)
3. Check browser console for JavaScript errors (F12)
4. Verify WebSocket is connected (green dot in status indicator)
5. Try logging out and back in

---

### Problem: "Rate Limit" Error

**Symptoms:** "ERR_RATE_LIMIT: Too many requests. Please slow down."

**Explanation:** You're sending more than 10 WebSocket messages per second. This is a security protection against abuse.

**Solutions:**
1. Stop clicking buttons rapidly
2. Wait a few seconds
3. Try again more slowly
4. If problem persists, reload page

---

## Tips and Best Practices

### For Operators

1. **Use Descriptive Club Names**
   - Good: "Eagles Badminton Club", "Morning Stars"
   - Bad: "Club 1", "Us", "Test"

2. **Check NTP Before Creating Schedules**
   - Always verify green ✓ before setting up automatic scheduling
   - If time is wrong, schedules will trigger at wrong times

3. **Test Your Schedules**
   - Create a test schedule for a few minutes in the future
   - Verify it triggers correctly
   - Delete test schedule after confirming

4. **Coordinate with Other Clubs**
   - While you can't see their schedules, communicate to avoid conflicts
   - Ask admin to check for conflicts if needed

5. **Change Your Password**
   - Change default password after first login
   - Use password at least 8 characters long
   - Don't share password with unauthorized people

6. **Document Your Schedules**
   - Keep a backup record of your club's schedule times
   - Useful if factory reset is needed

### For Admins

1. **Change Admin Password Immediately**
   - Default "admin"/"admin" is insecure
   - Use strong password (12+ characters)
   - Don't share admin password

2. **Create Descriptive Operator Usernames**
   - Use pattern like "club_clubname" or "coordinator_lastname"
   - Makes it easy to identify which club each operator manages

3. **Monitor Schedule Conflicts**
   - Regularly check calendar for overlapping bookings
   - Proactively contact clubs if conflicts arise

4. **Regular Password Rotations**
   - Ask operators to change passwords every 3-6 months
   - Change admin password regularly

5. **Document Operator Accounts**
   - Keep a list of which operator username belongs to which club
   - Include contact information for each operator
   - Useful when operator leaves and needs replacement

6. **Before Factory Reset**
   - Screenshot the calendar to preserve schedule information
   - Export or write down all operator usernames
   - Notify all operators that reset will occur
   - Only use as last resort

7. **Test NTP Status**
   - Verify time sync works after ESP32 reboots
   - Check timezone is correct (should show NZ time)

---

## Keyboard Shortcuts

For faster operation, you can use these keyboard shortcuts:

- **SPACE**: Pause/Resume timer
- **R**: Reset timer
- **S**: Start timer
- **M**: Mute/Unmute sound effects
- **?**: Show help

**Note:** Shortcuts only work when you're logged in as operator or admin (not in viewer mode).

---

## Need More Help?

- **Installation Issues**: See INSTALL_GUIDE.md
- **Technical/Development**: See ARCHITECTURE.md and API.md
- **Bug Reports**: Contact system administrator
- **Feature Requests**: Contact system administrator

---

**User Guide Version**: 3.0.0
**Last Updated**: 2025-10-30
**Project**: ESP32 Badminton Timer
**Author**: Claude Code
