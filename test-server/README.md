# ESP32 Badminton Timer - Local Test Server

This is a mock server that simulates the ESP32 hardware, allowing you to test the web interface locally before deploying to your ESP32 device.

## Prerequisites

You need Node.js installed on your computer. If you don't have it:

- **Windows/Mac**: Download from https://nodejs.org/ (LTS version recommended)
- **Linux**:
  ```bash
  sudo apt update
  sudo apt install nodejs npm
  ```

## Quick Start

1. **Install dependencies** (first time only):
   ```bash
   cd test-server
   npm install
   ```

2. **Start the server**:
   ```bash
   npm start
   ```

3. **Open your browser** to:
   ```
   http://localhost:8080
   ```

## Test Credentials

The mock server comes with pre-configured test accounts:

### Admin Account
- **Username**: `admin`
- **Password**: `admin`
- **Can do**: Everything - manage users, schedules, timer control

### Operator Account
- **Username**: `operator1`
- **Password**: `pass123`
- **Can do**: Create schedules for their club, control timer, change own password

### Viewer Mode
- **Username**: *(leave empty)*
- **Password**: *(leave empty)*
- **Can do**: View only - no controls

## What You Can Test

### ✅ Fully Functional Features:

1. **Authentication System**
   - Login as admin/operator/viewer
   - Role-based permissions
   - Session management

2. **Timer Controls**
   - Start/Stop timer
   - Set duration
   - Real-time countdown display

3. **Schedule Management**
   - Create/Edit/Delete schedules
   - Weekly calendar view
   - Enable/Disable scheduling
   - Club-based filtering (operators see only their schedules)

4. **User Management** (Admin only)
   - Add/Remove operators
   - View operator list
   - Change passwords

5. **NTP Status Display**
   - Shows simulated time sync status
   - Updates every 5 seconds

### ⚠️ Limitations (Hardware Required):

These features can't be fully tested without the ESP32:
- Actual siren/buzzer activation
- Real NTP time synchronization from internet
- Non-volatile storage (NVS) persistence
- WiFi connection management
- Schedule auto-triggering at specific times

## Mock Data

The server starts with 2 pre-configured test schedules:

1. **Test Club A**
   - Monday at 18:00
   - Duration: 90 minutes
   - Owner: operator1

2. **Test Club B**
   - Wednesday at 19:30
   - Duration: 60 minutes
   - Owner: operator1

## Server Console Output

The server logs all activity to the console:

```
🚀 Mock ESP32 Server running!
📱 Open your browser to: http://localhost:8080

✅ Client 1 connected
🔑 Client 1 logged in as ADMIN
⏱️  Timer started
📅 Scheduling enabled
➕ Schedule added: My Club
```

## Testing Scenarios

### Scenario 1: Admin Testing
1. Login as admin (`admin` / `admin`)
2. Go to Users page (person icon)
3. Add a new operator
4. Go to Schedules page (calendar icon)
5. View all schedules from all operators
6. Enable scheduling
7. Create a new schedule

### Scenario 2: Operator Testing
1. Login as operator (`operator1` / `pass123`)
2. Go to Schedules page
3. See only your schedules (not other operators')
4. Try to create/edit/delete schedules
5. Control the timer

### Scenario 3: Viewer Testing
1. Click "Continue as Viewer" (no credentials)
2. Notice all controls are disabled
3. Can only view timer and schedules
4. Cannot make any changes

### Scenario 4: Multi-User Testing
1. Open two browser windows
2. Login as admin in one, operator in other
3. Make changes in one window
4. See real-time updates in both windows

## Troubleshooting

### Port Already in Use
If port 8080 is already taken, edit `server.js` and change:
```javascript
const PORT = 8080;  // Change to 3000, 8000, etc.
```

### WebSocket Connection Failed
- Make sure the server is running (`npm start`)
- Check browser console for errors (F12)
- Try refreshing the page

### Changes Not Appearing
- Hard refresh your browser (Ctrl+Shift+R or Cmd+Shift+R)
- Clear browser cache
- Restart the server

## Differences from Real ESP32

| Feature | Mock Server | Real ESP32 |
|---------|------------|------------|
| Data persistence | In-memory (lost on restart) | NVS flash storage (permanent) |
| Time sync | System time | NTP from internet |
| Schedule triggers | Manual testing only | Automatic at scheduled time |
| Siren control | Simulated | Real GPIO output |
| Network | localhost only | WiFi accessible from network |

## Next Steps

Once you've tested the interface locally:

1. **Build and upload** to ESP32 using PlatformIO:
   ```bash
   pio run --target upload
   ```

2. **Upload filesystem** (web files):
   ```bash
   pio run --target uploadfs
   ```

3. **Connect to ESP32's WiFi** or configure your WiFi credentials

4. **Access the real device** at its IP address

## Development Tips

- The mock server hot-reloads - just refresh browser after changes
- Check browser console (F12) for JavaScript errors
- Check server terminal for backend logs
- Test on mobile devices by using your computer's IP: `http://192.168.x.x:8080`

## Files Structure

```
test-server/
├── server.js       # Mock WebSocket server
├── package.json    # Node.js dependencies
└── README.md       # This file

data/               # Web interface files (shared with ESP32)
├── index.html      # Main HTML structure
├── style.css       # All styling
└── script.js       # Client-side JavaScript
```

## Support

If something doesn't work as expected:
1. Check the browser console for JavaScript errors
2. Check the server terminal for backend errors
3. Compare behavior with this documentation
4. Test with a fresh browser session (incognito mode)
