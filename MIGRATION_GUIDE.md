# Migration Guide - Version 2.0

## Overview

Version 2.0 includes critical security improvements and timer synchronization fixes. **This is a breaking change** that requires action from existing users.

## Breaking Changes

### 1. Web Interface Authentication Required

**What changed:** The web interface now requires password authentication to control the timer.

**Why:** Previous versions allowed anyone on your local network to control the timer without authentication.

**Action required:**
1. Create `src/wifi_credentials.h` from the example file
2. Set a strong `WEB_PASSWORD`
3. Upload the updated firmware
4. When accessing the web interface, enter your password

### 2. OTA Password Now Configurable

**What changed:** OTA password is no longer hardcoded as "badminton"

**Why:** Hardcoded passwords are a security risk

**Action required:**
1. Set `OTA_PASSWORD` in `src/wifi_credentials.h`
2. Update your OTA upload scripts/commands to use the new password

### 3. WiFi Credentials File Structure Changed

**What changed:** `std::vector` replaced with static array for better memory safety

**Why:** Vectors can cause memory fragmentation on ESP32

**Action required:**
1. Update your `src/wifi_credentials.h` file to match the new format in `wifi_credentials.h.example`
2. The new format includes the `known_networks_count` variable

## New Features

### Enhanced Timer Synchronization

- **Smooth countdown:** Uses requestAnimationFrame for 60fps updates
- **No drift:** Server syncs every 5 seconds to prevent timer drift
- **No jitter or backwards jumps:** Eliminated timing issues

**No action required** - this works automatically after upgrade.

### Improved WiFi Portal Stability

- **Android/iOS support:** Captive portal detection prevents automatic disconnection
- **Longer timeout:** Portal timeout increased from 3 to 5 minutes
- **More retry attempts:** Better resilience to connection issues

**No action required** - this works automatically.

### Connection Status Indicator

- **Visual feedback:** Green dot = connected, Red blinking dot = disconnected
- **Located in top-right corner** of the web interface

**No action required** - visible immediately after upgrade.

### Better Error Handling

- **Input validation:** Server validates all settings changes
- **Error messages:** Clear error messages displayed when something goes wrong
- **Auto-reconnection:** WebSocket automatically reconnects with exponential backoff
- **Auto-restart:** ESP32 restarts automatically if critical errors occur

**No action required** - this works automatically.

## Step-by-Step Migration

### For New Installations

1. Clone/download the repository
2. Copy `src/wifi_credentials.h.example` to `src/wifi_credentials.h`
3. Edit `src/wifi_credentials.h`:
   - Set strong passwords for `OTA_PASSWORD` and `WEB_PASSWORD`
   - Add your WiFi networks
4. Build and upload firmware: `platformio run --target upload`
5. Upload filesystem: `platformio run --target uploadfs`
6. Connect to captive portal or wait for WiFi connection
7. Access web interface and enter your `WEB_PASSWORD`

### For Existing Users (Upgrading from v1.x)

#### Option A: Fresh Install (Recommended)

1. **Backup your current settings** (note down game duration, rounds, etc.)
2. **Create credentials file:**
   ```bash
   cp src/wifi_credentials.h.example src/wifi_credentials.h
   ```
3. **Edit `src/wifi_credentials.h`:**
   - Set `OTA_PASSWORD` to a strong password
   - Set `WEB_PASSWORD` to a strong password
   - Add your WiFi networks using the new array format
4. **Upload firmware:**
   ```bash
   platformio run --target upload
   platformio run --target uploadfs
   ```
5. **Access web interface** at `http://badminton-timer.local`
6. **Enter password** when prompted
7. **Restore your settings** from step 1

#### Option B: Update Existing Credentials File

If you already have `src/wifi_credentials.h`:

1. **Update the file format:**
   ```cpp
   // Add at the top:
   const char* OTA_PASSWORD = "YourStrongPassword123!";
   const char* WEB_PASSWORD = "YourWebPassword456!";

   // Change from:
   std::vector<WiFiCredential> known_networks = { ... };

   // To:
   const WiFiCredential known_networks[] = { ... };
   const size_t known_networks_count = sizeof(known_networks) / sizeof(WiFiCredential);
   ```

2. **Remove `#include <vector>` if present** at the top of the file

3. **Upload firmware:**
   ```bash
   platformio run --target upload
   platformio run --target uploadfs
   ```

## Troubleshooting

### "Authentication required" prompt appears repeatedly

**Cause:** Wrong password entered

**Solution:**
- Check your `src/wifi_credentials.h` for the correct `WEB_PASSWORD`
- Ensure you uploaded the firmware after setting the password
- Refresh the page and try again

### Can't connect to WiFi portal on Android

**Cause:** Android may still think there's no internet and disconnect

**Solution:**
- When connecting to `BadmintonTimerSetup`, tap "Stay connected" if prompted
- Disable "Smart Network Switch" in Android WiFi settings
- The new version has better Android support, but some devices are stubborn

### OTA upload fails with "authentication failed"

**Cause:** Using old password or wrong password

**Solution:**
- Check `OTA_PASSWORD` in `src/wifi_credentials.h`
- Ensure this password is used in your upload command
- In PlatformIO: Update `upload_flags` in `platformio.ini` if using OTA upload

### Timer display is jittery

**Cause:** May need to clear browser cache

**Solution:**
- Hard refresh the page (Ctrl+Shift+R or Cmd+Shift+R)
- Clear browser cache
- Try a different browser
- The new requestAnimationFrame code should eliminate jitter

### WebSocket disconnects frequently

**Cause:** Network issues or signal strength

**Solution:**
- The new version has automatic reconnection with exponential backoff
- Check your WiFi signal strength
- Try moving ESP32 closer to router
- Check serial monitor for connection errors

## Rollback Instructions

If you need to revert to the previous version:

```bash
git checkout <previous-commit-hash>
platformio run --target upload
platformio run --target uploadfs
```

**Note:** Rolling back will lose all Phase 1 improvements, including security fixes. Only do this if absolutely necessary.

## Support

If you encounter issues not covered in this guide:

1. Check the main README.md
2. Check serial monitor output for error messages
3. Review UPGRADE_PLAN.md for detailed technical information
4. Open an issue on GitHub with:
   - Error messages from serial monitor
   - Steps to reproduce
   - Your ESP32 board type
   - PlatformIO version

## What's Next?

Phase 2 improvements are planned, including:

- Code refactoring for better maintainability
- Performance optimizations
- Keyboard shortcuts
- Sound effects
- Comprehensive documentation
- Unit tests

See `UPGRADE_PLAN.md` for the complete roadmap.
