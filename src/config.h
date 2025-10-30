#pragma once

// =============================================================================
// ESP32 Badminton Timer - Configuration File
// =============================================================================

// =============================================================================
// Hardware Configuration
// =============================================================================

// Relay pin for siren control
constexpr int RELAY_PIN = 26;

// =============================================================================
// Timer Configuration
// =============================================================================

// Default timer settings
constexpr unsigned long DEFAULT_GAME_DURATION = 21 * 60 * 1000;  // 21 minutes in milliseconds
constexpr unsigned long DEFAULT_BREAK_DURATION = 60 * 1000;      // 60 seconds in milliseconds
constexpr unsigned int DEFAULT_NUM_ROUNDS = 3;                   // 3 rounds per match
constexpr bool DEFAULT_BREAK_TIMER_ENABLED = true;               // Break timer enabled by default
constexpr unsigned long DEFAULT_SIREN_LENGTH = 1000;             // 1 second siren blast
constexpr unsigned long DEFAULT_SIREN_PAUSE = 1000;              // 1 second pause between blasts

// Validation limits for timer settings
constexpr unsigned long MIN_GAME_DURATION_MIN = 1;               // Minimum game duration (minutes)
constexpr unsigned long MAX_GAME_DURATION_MIN = 120;             // Maximum game duration (minutes)
constexpr unsigned long MIN_BREAK_DURATION_SEC = 1;              // Minimum break duration (seconds)
constexpr unsigned long MAX_BREAK_DURATION_SEC = 3600;           // Maximum break duration (seconds)
constexpr unsigned int MIN_ROUNDS = 1;                           // Minimum number of rounds
constexpr unsigned int MAX_ROUNDS = 20;                          // Maximum number of rounds
constexpr unsigned long MIN_SIREN_LENGTH_MS = 100;               // Minimum siren blast length
constexpr unsigned long MAX_SIREN_LENGTH_MS = 10000;             // Maximum siren blast length
constexpr unsigned long MIN_SIREN_PAUSE_MS = 100;                // Minimum pause between blasts
constexpr unsigned long MAX_SIREN_PAUSE_MS = 10000;              // Maximum pause between blasts
constexpr float MAX_BREAK_PERCENTAGE = 0.5;                      // Break can be max 50% of game duration

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
constexpr const char* PREF_KEY_BREAK_DURATION = "breakDuration";
constexpr const char* PREF_KEY_NUM_ROUNDS = "numRounds";
constexpr const char* PREF_KEY_BREAK_ENABLED = "breakEnabled";
constexpr const char* PREF_KEY_SIREN_LENGTH = "sirenLength";
constexpr const char* PREF_KEY_SIREN_PAUSE = "sirenPause";

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
constexpr int MIN_PASSWORD_LENGTH = 4;                           // Minimum password length

// =============================================================================
// Schedule Configuration
// =============================================================================

// Schedule validation limits
constexpr int MIN_SCHEDULE_DURATION_MIN = 1;                     // Minimum 1 minute
constexpr int MAX_SCHEDULE_DURATION_MIN = 180;                   // Maximum 180 minutes (3 hours)

// Schedule trigger de-bounce
constexpr int SCHEDULE_TRIGGER_DEBOUNCE_MIN = 2;                 // Don't re-trigger within 2 minutes

// =============================================================================
// Hello Club API Configuration
// =============================================================================

// API retry settings
constexpr int HELLOCLUB_MAX_RETRIES = 3;                         // Maximum retry attempts
constexpr int HELLOCLUB_RETRY_DELAY_MS[] = {1000, 2000, 4000};   // Exponential backoff delays
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

constexpr const char* FIRMWARE_VERSION = "2.0.0";
constexpr const char* BUILD_DATE = __DATE__;
constexpr const char* BUILD_TIME = __TIME__;
