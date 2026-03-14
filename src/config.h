#pragma once

// =============================================================================
// ESP32 Badminton Timer - Configuration File
// =============================================================================

// =============================================================================
// Hardware Configuration
// =============================================================================

// Relay pin for siren control
constexpr int RELAY_PIN = 26;

// Factory reset button (BOOT button on most ESP32 boards)
constexpr int FACTORY_RESET_BUTTON_PIN = 0;                   // GPIO 0 (BOOT button)
constexpr unsigned long FACTORY_RESET_HOLD_TIME_MS = 10000;   // Hold for 10 seconds to factory reset
constexpr unsigned long BUTTON_DEBOUNCE_MS = 50;              // Button debounce time

// =============================================================================
// Timer Configuration
// =============================================================================

// Default timer settings
constexpr unsigned long DEFAULT_GAME_DURATION = 12 * 60 * 1000;  // 12 minutes in milliseconds
constexpr unsigned int DEFAULT_NUM_ROUNDS = 3;                   // 3 rounds per match
constexpr unsigned long DEFAULT_SIREN_LENGTH = 1000;             // 1 second siren blast
constexpr unsigned long DEFAULT_SIREN_PAUSE = 1000;              // 1 second pause between blasts

// Validation limits for timer settings
constexpr unsigned long MIN_GAME_DURATION_MIN = 1;               // Minimum game duration (minutes)
constexpr unsigned long MAX_GAME_DURATION_MIN = 120;             // Maximum game duration (minutes)
constexpr unsigned int MIN_ROUNDS = 1;                           // Minimum number of rounds
constexpr unsigned int MAX_ROUNDS = 20;                          // Maximum number of rounds
constexpr unsigned long MIN_SIREN_LENGTH_MS = 100;               // Minimum siren blast length
constexpr unsigned long MAX_SIREN_LENGTH_MS = 10000;             // Maximum siren blast length
constexpr unsigned long MIN_SIREN_PAUSE_MS = 100;                // Minimum pause between blasts
constexpr unsigned long MAX_SIREN_PAUSE_MS = 10000;              // Maximum pause between blasts

// =============================================================================
// Network Configuration
// =============================================================================

// WiFi connection settings
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 10000;         // 10 seconds per network
constexpr unsigned long CAPTIVE_PORTAL_TIMEOUT_SEC = 300;        // 5 minutes captive portal timeout
constexpr int WIFI_CONNECT_RETRIES = 5;                          // Number of retry attempts

// Captive portal settings
constexpr const char* AP_SSID = "BadmintonTimerSetup";          // Access point SSID
constexpr const char* AP_IP = "192.168.4.1";                    // Access point IP address

// mDNS settings
constexpr const char* MDNS_HOSTNAME = "badminton-timer";        // mDNS hostname (*.local)

// =============================================================================
// WebSocket Configuration
// =============================================================================

// WebSocket sync interval
constexpr unsigned long SYNC_INTERVAL_MS = 5000;                 // Sync every 5 seconds

// WebSocket reconnection settings
constexpr int MAX_WEBSOCKET_RECONNECT_ATTEMPTS = 10;             // Maximum reconnection attempts
constexpr unsigned long MIN_RECONNECT_DELAY_MS = 1000;           // Minimum reconnection delay
constexpr unsigned long MAX_RECONNECT_DELAY_MS = 30000;          // Maximum reconnection delay

// Maximum number of simultaneous WebSocket clients
constexpr size_t MAX_WEBSOCKET_CLIENTS = 10;                     // Limit concurrent connections

// =============================================================================
// JSON Configuration
// =============================================================================

// JSON document sizes for different message types
constexpr size_t JSON_DOC_SIZE_SMALL = 256;                      // For simple events
constexpr size_t JSON_DOC_SIZE_MEDIUM = 512;                     // For state/settings
constexpr size_t JSON_DOC_SIZE_LARGE = 1024;                     // For complex messages

// =============================================================================
// Preferences (NVS) Configuration
// =============================================================================

// Preferences namespace
constexpr const char* PREFERENCES_NAMESPACE = "timer";

// Preferences keys
constexpr const char* PREF_KEY_GAME_DURATION = "gameDuration";
constexpr const char* PREF_KEY_NUM_ROUNDS = "numRounds";
constexpr const char* PREF_KEY_SIREN_LENGTH = "sirenLength";
constexpr const char* PREF_KEY_SIREN_PAUSE = "sirenPause";
constexpr const char* PREF_KEY_HC_DEFAULT_DURATION = "hcDefDur";

// =============================================================================
// System Configuration
// =============================================================================

// Serial baud rate
constexpr unsigned long SERIAL_BAUD_RATE = 115200;

// Watchdog timer settings
constexpr unsigned long WATCHDOG_TIMEOUT_SEC = 30;               // 30 second watchdog timeout

// SPIFFS restart delay on failure
constexpr unsigned long SPIFFS_RESTART_DELAY_MS = 5000;          // 5 seconds

// Timezone
constexpr const char* TIMEZONE_LOCATION = "Pacific/Auckland";    // New Zealand timezone

// Session timeout
constexpr unsigned long SESSION_TIMEOUT_MS = 30 * 60 * 1000;     // 30 minutes session timeout
constexpr unsigned long SESSION_CHECK_INTERVAL_MS = 60000;       // Check sessions every 60 seconds

// Schedule trigger checking
constexpr unsigned long SCHEDULE_CHECK_INTERVAL_MS = 30000;      // Check every 30 seconds

// Rate limiting
constexpr int MAX_MESSAGES_PER_SECOND = 10;                      // Max messages per client per second

// =============================================================================
// User Management Configuration
// =============================================================================

// Password requirements
constexpr int MIN_PASSWORD_LENGTH = 8;                           // Minimum password length (increased for security)

// =============================================================================
// Hello Club API Configuration
// =============================================================================

// Polling intervals
constexpr unsigned long HELLOCLUB_POLL_INTERVAL_MS = 3600000;    // Poll every 1 hour
constexpr unsigned long HELLOCLUB_RETRY_INTERVAL_MS = 300000;    // Retry in 5 minutes on failure
constexpr int HELLOCLUB_DAYS_AHEAD = 7;                          // Look ahead 7 days
constexpr int HELLOCLUB_MAX_CACHED_EVENTS = 20;                  // Max cached events
constexpr unsigned long HELLOCLUB_TRIGGER_WINDOW_MS = 120000;    // 2 minute trigger window

// API retry settings
constexpr int HELLOCLUB_REQUEST_TIMEOUT_MS = 10000;              // 10 second timeout

// =============================================================================
// Debug Configuration
// =============================================================================

// Debug mode (set to 0 to disable, 1 to enable)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif

#if DEBUG_MODE
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(fmt, ...)
#endif

// =============================================================================
// Feature Flags
// =============================================================================

// Enable/disable optional features
constexpr bool ENABLE_WATCHDOG = true;                           // Enable watchdog timer
constexpr bool ENABLE_SELF_TEST = true;                          // Enable boot self-test
constexpr bool ENABLE_OTA = true;                                // Enable OTA updates
constexpr bool ENABLE_MDNS = true;                               // Enable mDNS discovery

// =============================================================================
// Version Information
// =============================================================================

constexpr const char* FIRMWARE_VERSION = "3.1.0";
constexpr const char* BUILD_DATE = __DATE__;
constexpr const char* BUILD_TIME = __TIME__;
