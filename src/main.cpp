#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include <ezTime.h>
#include <Preferences.h>
#include "ESPAsyncWiFiManager.h"
#include <ArduinoOTA.h>
#include <map>
#include "esp_task_wdt.h"
#include "wifi_credentials.h"
#include "config.h"
#include "timer.h"
#include "siren.h"
#include "settings.h"
#include "users.h"
#include "schedule.h"
#include "helloclub.h"

// ==========================================================================
// --- Hardware & Global State ---
// ==========================================================================

// Modular components
Timer timer;
Siren siren(RELAY_PIN);
Settings settings;
UserManager userManager;
ScheduleManager scheduleManager;
HelloClubClient helloClubClient;

// Timezone
Timezone myTZ;

// Web Server & WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dns;

// WebSocket Authentication (maps client ID to user role and username)
std::map<uint32_t, UserRole> authenticatedClients;
std::map<uint32_t, String> authenticatedUsernames;

// Rate Limiting
struct RateLimitInfo {
    unsigned long windowStart;
    int messageCount;
};
std::map<uint32_t, RateLimitInfo> clientRateLimits;
const int MAX_MESSAGES_PER_SECOND = 10;
const unsigned long RATE_LIMIT_WINDOW = 1000; // 1 second

// Session Timeout
std::map<uint32_t, unsigned long> clientLastActivity;
const unsigned long SESSION_TIMEOUT = 30 * 60 * 1000; // 30 minutes

// Periodic Sync
unsigned long lastSyncBroadcast = 0;

// NTP Sync Status Tracking
bool lastNTPSyncStatus = false;
unsigned long lastNTPStatusCheck = 0;
const unsigned long NTP_CHECK_INTERVAL = 5000; // Check every 5 seconds

// Factory Reset Button State
unsigned long factoryResetButtonPressStart = 0;
bool factoryResetButtonPressed = false;
bool factoryResetInProgress = false;

// Hello Club Integration
String helloClubApiKey = "";
bool helloClubEnabled = false;
int helloClubDaysAhead = 7;
String helloClubCategoryFilter = "";
int helloClubSyncHour = 0; // 0 = midnight
unsigned long lastHelloClubSync = 0;
int lastHelloClubSyncDay = -1; // Track which day we last synced

// ==========================================================================
// --- Function Declarations ---
// ==========================================================================

bool connectToKnownWiFi();
void sendEvent(const String& type);
void sendStateUpdate(AsyncWebSocketClient *client = nullptr);
void sendSettingsUpdate(AsyncWebSocketClient *client = nullptr);
void sendSync(AsyncWebSocketClient *client);
void sendError(AsyncWebSocketClient *client, const String& message);
void sendAuthRequest(AsyncWebSocketClient *client);
void sendNTPStatus(AsyncWebSocketClient *client = nullptr);
void checkAndBroadcastNTPStatus();
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void setupOTA();
void setupWatchdog();
void runSelfTest();
String getFormattedTime12Hour();
void loadHelloClubSettings();
void saveHelloClubSettings();
void checkDailyHelloClubSync();
bool syncHelloClubEvents(bool skipConflictCheck = false);

// ==========================================================================
// --- Setup ---
// ==========================================================================

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    DEBUG_PRINTLN("\n\n=================================");
    DEBUG_PRINTF("ESP32 Badminton Timer v%s\n", FIRMWARE_VERSION);
    DEBUG_PRINTF("Build: %s %s\n", BUILD_DATE, BUILD_TIME);
    DEBUG_PRINTLN("=================================\n");

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    // Configure factory reset button (BOOT button)
    pinMode(FACTORY_RESET_BUTTON_PIN, INPUT_PULLUP);
    DEBUG_PRINTLN("Factory reset button configured (hold BOOT button for 10 seconds)");

    // Configure watchdog timer
    if (ENABLE_WATCHDOG) {
        setupWatchdog();
    }

    // Run self-test
    if (ENABLE_SELF_TEST) {
        runSelfTest();
    }

    if (!SPIFFS.begin(true)) {
        DEBUG_PRINTLN("SPIFFS mount failed! Restarting in 5 seconds...");
        delay(SPIFFS_RESTART_DELAY_MS);
        ESP.restart();
    }

    // Initialize siren hardware
    siren.begin();

    // Load settings from NVS
    settings.load(timer, siren);

    // Initialize user management
    userManager.begin();

    // Initialize schedule management
    scheduleManager.begin();

    // Load Hello Club settings
    loadHelloClubSettings();

    if (!connectToKnownWiFi()) {
        // If connection to a known network fails, start the captive portal
        // First, add captive portal detection handlers for Android/iOS/Windows
        // These prevent devices from automatically disconnecting due to "no internet"

        // Android captive portal detection
        server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
            request->redirect("http://192.168.4.1");
        });
        server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request){
            request->redirect("http://192.168.4.1");
        });

        // iOS/macOS captive portal detection
        server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){
            request->redirect("http://192.168.4.1");
        });
        server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request){
            request->redirect("http://192.168.4.1");
        });

        // Windows connectivity check
        server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "Microsoft Connect Test");
        });
        server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "Microsoft NCSI");
        });

        AsyncWiFiManager wifiManager(&server, &dns);
        wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        wifiManager.setConfigPortalTimeout(300); // 5 minutes (increased from 3)
        wifiManager.setConnectRetries(5); // More retry attempts

        if (!wifiManager.autoConnect("BadmintonTimerSetup")) {
            Serial.println("Failed to connect via portal and hit timeout. Restarting...");
            delay(3000);
            ESP.restart();
        }
    }

    Serial.println("Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    setupOTA();

    if (!MDNS.begin("badminton-timer")) {
        Serial.println("Error setting up MDNS responder!");
    }
    MDNS.addService("http", "tcp", 80);

    // Handle favicon requests from browsers to prevent errors
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(204);
    });

    // Set timezone from settings and allow ezTime to sync in the background (non-blocking)
    String configuredTimezone = settings.getTimezone();
    myTZ.setLocation(configuredTimezone);
    Serial.printf("Timezone configured: %s\n", configuredTimezone.c_str());
    
    // Serve the main web page and static files
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });
    server.serveStatic("/", SPIFFS, "/");

    // Attach WebSocket events
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();
}

// ==========================================================================
// --- Main Loop ---
// ==========================================================================

void loop() {
    // Reset watchdog timer
    if (ENABLE_WATCHDOG) {
        esp_task_wdt_reset();
    }

    // Check for factory reset button press (BOOT button)
    // Button is active LOW (pressed = LOW, released = HIGH)
    if (!factoryResetInProgress) {
        bool buttonCurrentlyPressed = (digitalRead(FACTORY_RESET_BUTTON_PIN) == LOW);

        if (buttonCurrentlyPressed && !factoryResetButtonPressed) {
            // Button just pressed - start timing
            factoryResetButtonPressStart = millis();
            factoryResetButtonPressed = true;
            DEBUG_PRINTLN("Factory reset button pressed - hold for 10 seconds...");
        } else if (buttonCurrentlyPressed && factoryResetButtonPressed) {
            // Button being held - check duration
            unsigned long holdDuration = millis() - factoryResetButtonPressStart;

            // Provide feedback every 2 seconds
            static unsigned long lastFeedback = 0;
            if (holdDuration - lastFeedback >= 2000) {
                lastFeedback = holdDuration;
                DEBUG_PRINTF("Factory reset: %lu seconds...\n", holdDuration / 1000);

                // Blink relay as visual feedback
                digitalWrite(RELAY_PIN, HIGH);
                delay(100);
                digitalWrite(RELAY_PIN, LOW);
            }

            if (holdDuration >= FACTORY_RESET_HOLD_TIME_MS) {
                // Button held for required time - perform factory reset
                factoryResetInProgress = true;
                DEBUG_PRINTLN("\n=================================");
                DEBUG_PRINTLN("FACTORY RESET TRIGGERED!");
                DEBUG_PRINTLN("=================================\n");

                // Give visual and audio feedback
                for (int i = 0; i < 5; i++) {
                    digitalWrite(RELAY_PIN, HIGH);
                    delay(200);
                    digitalWrite(RELAY_PIN, LOW);
                    delay(200);
                }

                // Perform factory reset - reset all settings to defaults
                userManager.factoryReset();

                // Reset timer and siren to defaults
                timer.setGameDuration(DEFAULT_GAME_DURATION);
                timer.setBreakDuration(DEFAULT_BREAK_DURATION);
                timer.setNumRounds(DEFAULT_NUM_ROUNDS);
                timer.setBreakTimerEnabled(DEFAULT_BREAK_TIMER_ENABLED);
                siren.setBlastLength(DEFAULT_SIREN_LENGTH);
                siren.setBlastPause(DEFAULT_SIREN_PAUSE);
                settings.save(timer, siren);
                timer.reset();

                // Clear all schedules
                std::vector<Schedule> allSchedules = scheduleManager.getAllSchedules();
                for (const auto& schedule : allSchedules) {
                    scheduleManager.deleteSchedule(schedule.id);
                }
                scheduleManager.setSchedulingEnabled(false);

                // Clear Hello Club settings
                Preferences prefs;
                prefs.begin("helloclub", false);
                prefs.clear();
                prefs.end();

                DEBUG_PRINTLN("Factory reset complete. Restarting in 3 seconds...");
                delay(3000);
                ESP.restart();
            }
        } else if (!buttonCurrentlyPressed && factoryResetButtonPressed) {
            // Button released before timeout
            unsigned long holdDuration = millis() - factoryResetButtonPressStart;
            DEBUG_PRINTF("Factory reset cancelled (held for %lu ms)\n", holdDuration);
            factoryResetButtonPressed = false;
            factoryResetButtonPressStart = 0;
        }
    }

    events(); // Process ezTime events for time synchronization
    ArduinoOTA.handle(); // Handle Over-the-Air update requests
    ws.cleanupClients(); // Clean up disconnected WebSocket clients

    // Check and broadcast NTP sync status changes
    checkAndBroadcastNTPStatus();

    // Update siren state (non-blocking)
    siren.update();

    // Check for session timeouts (every 60 seconds)
    static unsigned long lastSessionCheck = 0;
    if (millis() - lastSessionCheck >= 60000) { // Check every minute
        lastSessionCheck = millis();
        unsigned long now = millis();

        // Check each client for session timeout
        for (auto it = clientLastActivity.begin(); it != clientLastActivity.end(); ) {
            uint32_t clientId = it->first;
            unsigned long lastActivity = it->second;

            // Check if session timed out (30 minutes of inactivity)
            if (now - lastActivity >= SESSION_TIMEOUT) {
                // Only timeout authenticated sessions (not viewers)
                if (authenticatedClients.find(clientId) != authenticatedClients.end() &&
                    authenticatedClients[clientId] != VIEWER) {

                    Serial.printf("Session timeout for client #%u\n", clientId);

                    // Downgrade to viewer (don't disconnect, just require re-auth)
                    authenticatedClients[clientId] = VIEWER;
                    authenticatedUsernames.erase(clientId);

                    // Send timeout notification
                    StaticJsonDocument<128> timeoutDoc;
                    timeoutDoc["event"] = "session_timeout";
                    timeoutDoc["message"] = "Session expired. Please login again.";
                    String output;
                    serializeJson(timeoutDoc, output);

                    AsyncWebSocketClient *timeoutClient = ws.client(clientId);
                    if (timeoutClient) {
                        timeoutClient->text(output);
                    }
                }

                it = clientLastActivity.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Check for scheduled timer starts (every 30 seconds for better reliability)
    static unsigned long lastScheduleCheck = 0;
    if (millis() - lastScheduleCheck >= 30000) { // Check every 30 seconds (reduced from 60s)
        lastScheduleCheck = millis();

        Schedule triggeredSchedule;
        if (scheduleManager.checkScheduleTrigger(myTZ, triggeredSchedule)) {
            // A schedule should trigger!
            DEBUG_PRINTF("Schedule triggered: %s for %s\n",
                         triggeredSchedule.id.c_str(),
                         triggeredSchedule.clubName.c_str());

            // Only auto-start if timer is not already running
            if (timer.getState() == IDLE || timer.getState() == FINISHED) {
                // Set timer duration to schedule duration
                timer.setGameDuration(triggeredSchedule.durationMinutes * 60000);
                timer.setBreakDuration(0); // No break for scheduled sessions
                timer.setNumRounds(1); // Single round
                timer.start();

                // Mark as triggered
                scheduleManager.markTriggered(triggeredSchedule.id, scheduleManager.getCurrentWeekMinute(myTZ));

                // Notify all clients
                StaticJsonDocument<512> scheduleStartDoc;
                scheduleStartDoc["event"] = "schedule_started";
                scheduleStartDoc["scheduleId"] = triggeredSchedule.id;
                scheduleStartDoc["clubName"] = triggeredSchedule.clubName;
                scheduleStartDoc["duration"] = triggeredSchedule.durationMinutes;
                String output;
                serializeJson(scheduleStartDoc, output);
                ws.textAll(output);

                DEBUG_PRINTLN("Timer auto-started by schedule");
            } else {
                DEBUG_PRINTLN("Timer already running, skipping schedule trigger");
            }
        }
    }

    // Check for daily Hello Club sync (once per minute)
    checkDailyHelloClubSync();

    // Update timer state
    if (timer.update()) {
        // Timer state changed - check what happened

        if (timer.hasBreakEnded()) {
            siren.start(1); // End of break: 1 blast
        }

        if (timer.hasRoundEnded()) {
            if (timer.isMatchFinished()) {
                // Match completed - play 3-blast fanfare
                siren.start(3);
                sendEvent("finished");
                DEBUG_PRINTLN("Match completed! All rounds finished.");
            } else {
                // Round ended but more rounds to go - play 2 blasts
                siren.start(2);

                StaticJsonDocument<256> roundDoc;
                roundDoc["event"] = "new_round";
                roundDoc["gameDuration"] = timer.getGameDuration();
                roundDoc["breakDuration"] = timer.getBreakDuration();
                roundDoc["currentRound"] = timer.getCurrentRound();
                roundDoc["numRounds"] = timer.getNumRounds();
                String output;
                serializeJson(roundDoc, output);
                ws.textAll(output);
            }
            sendStateUpdate();
        }
    }

    // Periodic sync broadcast to keep clients synchronized
    if (timer.getState() == RUNNING) {
        unsigned long now = millis();
        if (now - lastSyncBroadcast >= SYNC_INTERVAL_MS) {
            sendSync(nullptr); // Send to all clients
            lastSyncBroadcast = now;
        }
    }
    
    // The main loop is now much simpler. It only handles the core timer logic
    // and the siren. All notifications are sent via event-based functions.
}

// ==========================================================================
// --- Core Functions ---
// ==========================================================================

/**
 * @brief Attempts to connect to a list of pre-defined WiFi networks.
 * @return True if connection is successful, false otherwise.
 */
bool connectToKnownWiFi() {
    Serial.println("Trying to connect to a known WiFi network...");
    WiFi.mode(WIFI_STA);

    for (size_t i = 0; i < known_networks_count; i++) {
        const WiFiCredential& cred = known_networks[i];
        Serial.print("Connecting to: ");
        Serial.println(cred.ssid);

        // For networks with no password, the second argument can be NULL or an empty string
        WiFi.begin(cred.ssid, (strlen(cred.password) > 0) ? cred.password : NULL);

        // Wait for connection for up to 10 seconds
        unsigned long startAttemptTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Connection successful!");
            return true;
        } else {
            Serial.println("Connection failed.");
            WiFi.disconnect(); // Important to disconnect before trying the next one
        }
    }
    return false;
}

// ==========================================================================
// --- Helper Functions ---
// ==========================================================================

/**
 * @brief Formats the current time into a 12-hour AM/PM string.
 * @return The formatted time string.
 */
String getFormattedTime12Hour() {
    return myTZ.dateTime("h:i:s a");
}

// ==========================================================================
// --- WebSocket Communication ---
// ==========================================================================

void sendEvent(const String& type) {
    StaticJsonDocument<256> doc;
    doc["event"] = type;
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

void sendStateUpdate(AsyncWebSocketClient *client) {
    StaticJsonDocument<512> doc;
    doc["event"] = "state";
    JsonObject state = doc.createNestedObject("state");

    TimerState timerState = timer.getState();
    state["status"] = (timerState == RUNNING) ? "RUNNING" : ((timerState == PAUSED) ? "PAUSED" : "IDLE");
    state["mainTimer"] = (timerState == RUNNING || timerState == PAUSED) ? timer.getMainTimerRemaining() : timer.getGameDuration();
    state["breakTimer"] = (timerState == RUNNING || timerState == PAUSED) ? timer.getBreakTimerRemaining() : timer.getBreakDuration();
    state["currentRound"] = timer.getCurrentRound();
    state["numRounds"] = timer.getNumRounds();
    state["time"] = getFormattedTime12Hour();

    String output;
    serializeJson(doc, output);
    if (client) {
        client->text(output);
    } else {
        ws.textAll(output);
    }
}

void sendSettingsUpdate(AsyncWebSocketClient *client) {
    StaticJsonDocument<512> doc;
    doc["event"] = "settings";
    JsonObject settingsObj = doc.createNestedObject("settings");
    settingsObj["gameDuration"] = timer.getGameDuration();
    settingsObj["breakDuration"] = timer.getBreakDuration();
    settingsObj["numRounds"] = timer.getNumRounds();
    settingsObj["breakTimerEnabled"] = timer.isBreakTimerEnabled();
    settingsObj["sirenLength"] = siren.getBlastLength();
    settingsObj["sirenPause"] = siren.getBlastPause();

    String output;
    serializeJson(doc, output);
    if (client) {
        client->text(output);
    } else {
        ws.textAll(output);
    }
}

void sendSync(AsyncWebSocketClient *client) {
    StaticJsonDocument<512> syncDoc;
    syncDoc["event"] = "sync";
    syncDoc["mainTimerRemaining"] = timer.getMainTimerRemaining();
    syncDoc["breakTimerRemaining"] = timer.getBreakTimerRemaining();
    syncDoc["serverMillis"] = millis(); // Server timestamp for client sync
    syncDoc["currentRound"] = timer.getCurrentRound();
    syncDoc["numRounds"] = timer.getNumRounds();
    syncDoc["status"] = (timer.getState() == PAUSED) ? "PAUSED" : "RUNNING";

    String output;
    serializeJson(syncDoc, output);

    if (client) {
        client->text(output); // Send to specific client
    } else {
        ws.textAll(output); // Broadcast to all clients
    }
}

void sendError(AsyncWebSocketClient *client, const String& message) {
    if (!client) return;

    StaticJsonDocument<256> doc;
    doc["event"] = "error";
    doc["message"] = message;

    String output;
    serializeJson(doc, output);
    client->text(output);
}

void sendAuthRequest(AsyncWebSocketClient *client) {
    if (!client) return;

    StaticJsonDocument<256> doc;
    doc["event"] = "auth_required";
    doc["message"] = "Please enter password to control timer";

    String output;
    serializeJson(doc, output);
    client->text(output);
}

void sendNTPStatus(AsyncWebSocketClient *client) {
    StaticJsonDocument<512> doc;
    doc["event"] = "ntp_status";

    // Check if time is valid - more robust than just year check
    // ezTime: year() returns reasonable value AND hour/minute are valid
    bool synced = (myTZ.year() > 2020 && myTZ.year() < 2100 &&
                   myTZ.month() >= 1 && myTZ.month() <= 12 &&
                   myTZ.day() >= 1 && myTZ.day() <= 31);

    doc["synced"] = synced;
    doc["time"] = synced ? getFormattedTime12Hour() : "Not synced";

    // Add timezone info if synced
    if (synced) {
        doc["timezone"] = settings.getTimezone();
        doc["dateTime"] = myTZ.dateTime("Y-m-d H:i:s"); // ISO format for verification
        // Note: ezTime syncs every 30 minutes automatically via events() call
        doc["autoSyncInterval"] = 30; // minutes
    }

    String output;
    serializeJson(doc, output);

    if (client) {
        client->text(output);
    } else {
        ws.textAll(output);
    }
}

void checkAndBroadcastNTPStatus() {
    unsigned long now = millis();

    // Check every NTP_CHECK_INTERVAL milliseconds
    if (now - lastNTPStatusCheck < NTP_CHECK_INTERVAL) {
        return;
    }

    lastNTPStatusCheck = now;

    // Check if sync status changed - use same robust check as sendNTPStatus
    bool currentSyncStatus = (myTZ.year() > 2020 && myTZ.year() < 2100 &&
                              myTZ.month() >= 1 && myTZ.month() <= 12 &&
                              myTZ.day() >= 1 && myTZ.day() <= 31);

    if (currentSyncStatus != lastNTPSyncStatus) {
        lastNTPSyncStatus = currentSyncStatus;
        sendNTPStatus(nullptr);  // Broadcast to all clients
    }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client) {
    // Rate limiting check
    unsigned long now = millis();
    uint32_t clientId = client->id();

    if (clientRateLimits.find(clientId) == clientRateLimits.end()) {
        clientRateLimits[clientId] = {now, 0};
    }

    RateLimitInfo& rateLimit = clientRateLimits[clientId];

    // Reset window if it expired
    if (now - rateLimit.windowStart >= RATE_LIMIT_WINDOW) {
        rateLimit.windowStart = now;
        rateLimit.messageCount = 0;
    }

    rateLimit.messageCount++;

    // Check if rate limit exceeded
    if (rateLimit.messageCount > MAX_MESSAGES_PER_SECOND) {
        Serial.printf("Rate limit exceeded for client #%u (%d msgs/sec)\n", clientId, rateLimit.messageCount);
        sendError(client, "ERR_RATE_LIMIT: Too many requests. Please slow down.");
        return;
    }

    // Update last activity for session timeout
    clientLastActivity[clientId] = now;

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        Serial.println(F("deserializeJson() failed"));
        sendError(client, "ERR_INVALID_JSON: Invalid message format");
        return;
    }

    String action = doc["action"];

    // Get client's role (default to VIEWER if not authenticated)
    UserRole clientRole = VIEWER;
    if (authenticatedClients.find(client->id()) != authenticatedClients.end()) {
        clientRole = authenticatedClients[client->id()];
    }

    // Handle authentication
    if (action == "authenticate") {
        String username = doc["username"] | "";
        String password = doc["password"] | "";

        // Explicit viewer mode (empty credentials)
        if (username.isEmpty() && password.isEmpty()) {
            authenticatedClients[client->id()] = VIEWER;
            authenticatedUsernames[client->id()] = "Viewer";

            StaticJsonDocument<256> viewerDoc;
            viewerDoc["event"] = "viewer_mode";
            viewerDoc["role"] = "viewer";
            viewerDoc["username"] = "Viewer";
            viewerDoc["message"] = "Continuing as viewer (read-only access)";
            String output;
            serializeJson(viewerDoc, output);
            client->text(output);

            // Send initial state for viewers
            sendSettingsUpdate(client);
            if (timer.getState() == RUNNING || timer.getState() == PAUSED) {
                sendSync(client);
            } else {
                sendStateUpdate(client);
            }
            return;
        }

        // Attempt authentication with credentials
        UserRole role = userManager.authenticate(username, password);

        if (role != VIEWER) {
            // Successful authentication as operator or admin
            authenticatedClients[client->id()] = role;
            authenticatedUsernames[client->id()] = username;

            StaticJsonDocument<256> authDoc;
            authDoc["event"] = "auth_success";
            authDoc["role"] = (role == ADMIN) ? "admin" : "operator";
            authDoc["username"] = username;
            String output;
            serializeJson(authDoc, output);
            client->text(output);

            // Send initial state
            sendSettingsUpdate(client);
            if (timer.getState() == RUNNING || timer.getState() == PAUSED) {
                sendSync(client);
            } else {
                sendStateUpdate(client);
            }
        } else {
            // Authentication failed - credentials provided but incorrect
            sendError(client, "ERR_AUTH_FAILED: Invalid username or password");
        }
        return;
    }

    // Actions requiring OPERATOR or higher
    const bool needsOperator = (action == "start" || action == "pause" || action == "reset" || action == "save_settings");
    if (needsOperator && clientRole < OPERATOR) {
        sendError(client, "Operator access required");
        return;
    }

    // Actions requiring ADMIN
    const bool needsAdmin = (action == "add_operator" || action == "remove_operator" ||
                             action == "change_password" || action == "factory_reset" ||
                             action == "get_operators" || action == "set_timezone");
    if (needsAdmin && clientRole < ADMIN) {
        sendError(client, "Admin access required");
        return;
    }

    if (action == "start") {
        if (timer.getState() == RUNNING || timer.getState() == PAUSED) {
            sendError(client, "Timer already active. Reset first.");
            return;
        }
        if (timer.getState() == IDLE || timer.getState() == FINISHED) {
            timer.start();

            StaticJsonDocument<256> startDoc;
            startDoc["event"] = "start";
            startDoc["gameDuration"] = timer.getGameDuration();
            startDoc["breakDuration"] = timer.getBreakDuration();
            startDoc["numRounds"] = timer.getNumRounds();
            startDoc["currentRound"] = timer.getCurrentRound();
            String output;
            serializeJson(startDoc, output);
            ws.textAll(output);
        }
    } else if (action == "pause") {
        if (timer.getState() == RUNNING) {
            timer.pause();
            sendEvent("pause");
        } else if (timer.getState() == PAUSED) {
            timer.resume();
            sendEvent("resume");
        }
    } else if (action == "reset") {
        timer.reset();
        sendEvent("reset");
    } else if (action == "save_settings") {
        JsonObject settings = doc["settings"];

        // Validate and clamp input values
        unsigned long gameDur = settings["gameDuration"].as<unsigned long>();
        unsigned long breakDur = settings["breakDuration"].as<unsigned long>();
        unsigned int rounds = settings["numRounds"].as<unsigned int>();
        unsigned long sirenLen = settings["sirenLength"].as<unsigned long>();
        unsigned long sirenPau = settings["sirenPause"].as<unsigned long>();

        // Validation: Game duration (1-120 minutes)
        if (gameDur < 1 || gameDur > 120) {
            sendError(client, "Game duration must be between 1 and 120 minutes");
            return;
        }

        // Validation: Number of rounds (1-20)
        if (rounds < 1 || rounds > 20) {
            sendError(client, "Number of rounds must be between 1 and 20");
            return;
        }

        // Validation: Break duration (1-3600 seconds)
        if (breakDur < 1 || breakDur > 3600) {
            sendError(client, "Break duration must be between 1 and 3600 seconds");
            return;
        }

        // Validation: Break duration can't exceed 50% of game duration
        unsigned long gameDurInSeconds = gameDur * 60;
        if (breakDur > gameDurInSeconds / 2) {
            sendError(client, "Break duration cannot exceed 50% of game duration");
            return;
        }

        // Validation: Siren settings (100-10000 ms)
        if (sirenLen < 100 || sirenLen > 10000) {
            sendError(client, "Siren length must be between 100 and 10000 ms");
            return;
        }
        if (sirenPau < 100 || sirenPau > 10000) {
            sendError(client, "Siren pause must be between 100 and 10000 ms");
            return;
        }

        // All validation passed, apply to timer and siren
        timer.setGameDuration(gameDur * 60000);
        timer.setBreakDuration(breakDur * 1000);
        timer.setNumRounds(rounds);
        timer.setBreakTimerEnabled(settings["breakTimerEnabled"].as<bool>();
        siren.setBlastLength(sirenLen);
        siren.setBlastPause(sirenPau);

        // Save to NVS
        settings.save(timer, siren);
        sendSettingsUpdate(); // Notify all clients of the new settings
    } else if (action == "set_timezone") {
        String timezone = doc["timezone"] | "";

        if (timezone.length() == 0) {
            sendError(client, "Timezone cannot be empty");
            return;
        }

        // Set and save the new timezone
        if (settings.setTimezone(timezone)) {
            // Update ezTime timezone immediately
            myTZ.setLocation(timezone);

            Serial.printf("Timezone changed to: %s\n", timezone.c_str());

            // Confirm to client
            StaticJsonDocument<256> successDoc;
            successDoc["event"] = "timezone_changed";
            successDoc["timezone"] = timezone;
            successDoc["message"] = "Timezone updated successfully. Please refresh schedules.";
            String output;
            serializeJson(successDoc, output);
            client->text(output);

            // Notify all clients to update their NTP status
            sendNTPStatus();
        } else {
            sendError(client, "Failed to set timezone");
        }
    } else if (action == "add_operator") {
        String username = doc["username"] | "";
        String password = doc["password"] | "";

        if (userManager.addOperator(username, password)) {
            StaticJsonDocument<256> successDoc;
            successDoc["event"] = "operator_added";
            successDoc["username"] = username;
            String output;
            serializeJson(successDoc, output);
            client->text(output);
        } else {
            sendError(client, "Failed to add operator. Check username/password.");
        }
    } else if (action == "remove_operator") {
        String username = doc["username"] | "";

        if (userManager.removeOperator(username)) {
            StaticJsonDocument<256> successDoc;
            successDoc["event"] = "operator_removed";
            successDoc["username"] = username;
            String output;
            serializeJson(successDoc, output);
            client->text(output);
        } else {
            sendError(client, "Failed to remove operator. User not found.");
        }
    } else if (action == "change_password") {
        String username = doc["username"] | "";
        String oldPassword = doc["oldPassword"] | "";
        String newPassword = doc["newPassword"] | "";

        if (userManager.changePassword(username, oldPassword, newPassword)) {
            StaticJsonDocument<256> successDoc;
            successDoc["event"] = "password_changed";
            successDoc["message"] = "Password changed successfully";
            String output;
            serializeJson(successDoc, output);
            client->text(output);
        } else {
            sendError(client, "Failed to change password. Check credentials.");
        }
    } else if (action == "get_operators") {
        std::vector<String> operators = userManager.getOperators();

        StaticJsonDocument<512> opDoc;
        opDoc["event"] = "operators_list";
        JsonArray opArray = opDoc.createNestedArray("operators");
        for (const auto& op : operators) {
            opArray.add(op);
        }
        String output;
        serializeJson(opDoc, output);
        client->text(output);
    } else if (action == "factory_reset") {
        // Perform factory reset
        userManager.factoryReset();
        timer.setGameDuration(DEFAULT_GAME_DURATION);
        timer.setBreakDuration(DEFAULT_BREAK_DURATION);
        timer.setNumRounds(DEFAULT_NUM_ROUNDS);
        timer.setBreakTimerEnabled(DEFAULT_BREAK_TIMER_ENABLED);
        siren.setBlastLength(DEFAULT_SIREN_LENGTH);
        siren.setBlastPause(DEFAULT_SIREN_PAUSE);
        settings.save(timer, siren);
        timer.reset();

        StaticJsonDocument<256> resetDoc;
        resetDoc["event"] = "factory_reset_complete";
        resetDoc["message"] = "System reset to factory defaults";
        String output;
        serializeJson(resetDoc, output);
        ws.textAll(output); // Notify all clients

        // Clear all authenticated clients except viewers
        for (auto it = authenticatedClients.begin(); it != authenticatedClients.end();) {
            if (it->second != VIEWER) {
                it = authenticatedClients.erase(it);
            } else {
                ++it;
            }
        }

        sendSettingsUpdate();
    } else if (action == "get_schedules") {
        String clubFilter = doc["clubName"] | "";
        std::vector<Schedule> schedules;

        if (clientRole == ADMIN || clubFilter.isEmpty()) {
            // Admin can see all, or no filter specified
            schedules = scheduleManager.getAllSchedules();
        } else {
            // Operator can only see their club's schedules
            schedules = scheduleManager.getSchedulesByClub(clubFilter);
        }

        // Buffer size calculation: 50 schedules * 150 bytes/schedule ≈ 7500 bytes
        // Adding 500 byte overhead = 8000 bytes total, round up to 8192
        // Use DynamicJsonDocument to prevent stack overflow with many schedules
        DynamicJsonDocument schedulesDoc(8192);
        schedulesDoc["event"] = "schedules_list";
        schedulesDoc["schedulingEnabled"] = scheduleManager.isSchedulingEnabled();
        JsonArray schedulesArray = schedulesDoc.createNestedArray("schedules");

        for (const auto& sched : schedules) {
            JsonObject schedObj = schedulesArray.createNestedObject();
            schedObj["id"] = sched.id;
            schedObj["clubName"] = sched.clubName;
            schedObj["ownerUsername"] = sched.ownerUsername;
            schedObj["dayOfWeek"] = sched.dayOfWeek;
            schedObj["startHour"] = sched.startHour;
            schedObj["startMinute"] = sched.startMinute;
            schedObj["durationMinutes"] = sched.durationMinutes;
            schedObj["enabled"] = sched.enabled;
        }

        String output;
        serializeJson(schedulesDoc, output);
        client->text(output);
    } else if (action == "add_schedule") {
        // Check permission first
        if (clientRole < OPERATOR) {
            sendError(client, "Operator access required");
            return;
        }

        // Read schedule data from nested "schedule" object
        JsonObject schedObj = doc["schedule"];
        if (schedObj.isNull()) {
            sendError(client, "Missing schedule data");
            return;
        }

        Schedule newSchedule;
        newSchedule.id = scheduleManager.generateScheduleId();
        newSchedule.clubName = schedObj["clubName"] | "";
        newSchedule.dayOfWeek = schedObj["dayOfWeek"] | 0;
        newSchedule.startHour = schedObj["startHour"] | 0;
        newSchedule.startMinute = schedObj["startMinute"] | 0;
        newSchedule.durationMinutes = schedObj["durationMinutes"] | 60;
        newSchedule.enabled = schedObj["enabled"] | true;
        newSchedule.createdAt = millis();

        // Set owner to the authenticated user's actual username
        if (authenticatedUsernames.find(client->id()) != authenticatedUsernames.end()) {
            newSchedule.ownerUsername = authenticatedUsernames[client->id()];
        } else {
            newSchedule.ownerUsername = "unknown";
        }

        if (scheduleManager.addSchedule(newSchedule)) {
            StaticJsonDocument<512> successDoc;
            successDoc["event"] = "schedule_added";
            JsonObject scheduleObj = successDoc.createNestedObject("schedule");
            scheduleObj["id"] = newSchedule.id;
            scheduleObj["clubName"] = newSchedule.clubName;
            scheduleObj["ownerUsername"] = newSchedule.ownerUsername;
            scheduleObj["dayOfWeek"] = newSchedule.dayOfWeek;
            scheduleObj["startHour"] = newSchedule.startHour;
            scheduleObj["startMinute"] = newSchedule.startMinute;
            scheduleObj["durationMinutes"] = newSchedule.durationMinutes;
            scheduleObj["enabled"] = newSchedule.enabled;
            String output;
            serializeJson(successDoc, output);
            client->text(output);
        } else {
            sendError(client, "Failed to add schedule");
        }
    } else if (action == "update_schedule") {
        // Check permission first
        if (clientRole < OPERATOR) {
            sendError(client, "Operator access required");
            return;
        }

        // Read schedule data from nested "schedule" object
        JsonObject schedObj = doc["schedule"];
        if (schedObj.isNull()) {
            sendError(client, "Missing schedule data");
            return;
        }

        String scheduleId = schedObj["id"] | "";
        if (scheduleId.isEmpty()) {
            sendError(client, "Missing schedule ID");
            return;
        }

        // Get existing schedule to verify ownership and preserve owner
        Schedule existingSchedule;
        if (!scheduleManager.getScheduleById(scheduleId, existingSchedule)) {
            sendError(client, "Schedule not found");
            return;
        }

        // Get current user's username for permission check
        String currentUsername = "";
        if (authenticatedUsernames.find(client->id()) != authenticatedUsernames.end()) {
            currentUsername = authenticatedUsernames[client->id()];
        }

        // Check permission (admin can edit all, operator can only edit their own)
        if (!scheduleManager.hasPermission(existingSchedule, currentUsername, clientRole == ADMIN)) {
            sendError(client, "Permission denied - you can only edit your own schedules");
            return;
        }

        // Build updated schedule, preserving owner and creation time
        Schedule updatedSchedule;
        updatedSchedule.id = scheduleId;
        updatedSchedule.clubName = schedObj["clubName"] | "";
        updatedSchedule.dayOfWeek = schedObj["dayOfWeek"] | 0;
        updatedSchedule.startHour = schedObj["startHour"] | 0;
        updatedSchedule.startMinute = schedObj["startMinute"] | 0;
        updatedSchedule.durationMinutes = schedObj["durationMinutes"] | 60;
        updatedSchedule.enabled = schedObj["enabled"] | true;

        // SECURITY: Never accept ownerUsername from client - preserve existing owner
        updatedSchedule.ownerUsername = existingSchedule.ownerUsername;
        updatedSchedule.createdAt = existingSchedule.createdAt;

        // Validate schedule fields
        if (updatedSchedule.dayOfWeek < 0 || updatedSchedule.dayOfWeek > 6) {
            sendError(client, "Invalid day of week (must be 0-6)");
            return;
        }
        if (updatedSchedule.startHour < 0 || updatedSchedule.startHour > 23) {
            sendError(client, "Invalid hour (must be 0-23)");
            return;
        }
        if (updatedSchedule.startMinute < 0 || updatedSchedule.startMinute > 59) {
            sendError(client, "Invalid minute (must be 0-59)");
            return;
        }
        if (updatedSchedule.durationMinutes < 1 || updatedSchedule.durationMinutes > 180) {
            sendError(client, "Invalid duration (must be 1-180 minutes)");
            return;
        }

        if (scheduleManager.updateSchedule(updatedSchedule)) {
            StaticJsonDocument<512> successDoc;
            successDoc["event"] = "schedule_updated";
            JsonObject scheduleObj = successDoc.createNestedObject("schedule");
            scheduleObj["id"] = updatedSchedule.id;
            scheduleObj["clubName"] = updatedSchedule.clubName;
            scheduleObj["ownerUsername"] = updatedSchedule.ownerUsername;
            scheduleObj["dayOfWeek"] = updatedSchedule.dayOfWeek;
            scheduleObj["startHour"] = updatedSchedule.startHour;
            scheduleObj["startMinute"] = updatedSchedule.startMinute;
            scheduleObj["durationMinutes"] = updatedSchedule.durationMinutes;
            scheduleObj["enabled"] = updatedSchedule.enabled;
            String output;
            serializeJson(successDoc, output);
            client->text(output);
        } else {
            sendError(client, "Failed to update schedule");
        }
    } else if (action == "delete_schedule") {
        // Check permission first
        if (clientRole < OPERATOR) {
            sendError(client, "Operator access required");
            return;
        }

        String scheduleId = doc["id"] | "";
        if (scheduleId.isEmpty()) {
            sendError(client, "Missing schedule ID");
            return;
        }

        Schedule existingSchedule;
        if (!scheduleManager.getScheduleById(scheduleId, existingSchedule)) {
            sendError(client, "Schedule not found");
            return;
        }

        // Get current user's username for permission check
        String currentUsername = "";
        if (authenticatedUsernames.find(client->id()) != authenticatedUsernames.end()) {
            currentUsername = authenticatedUsernames[client->id()];
        }

        // Check permission (admin can delete all, operator can only delete their own)
        if (!scheduleManager.hasPermission(existingSchedule, currentUsername, clientRole == ADMIN)) {
            sendError(client, "Permission denied - you can only delete your own schedules");
            return;
        }

        if (scheduleManager.deleteSchedule(scheduleId)) {
            StaticJsonDocument<256> successDoc;
            successDoc["event"] = "schedule_deleted";
            successDoc["id"] = scheduleId;
            String output;
            serializeJson(successDoc, output);
            client->text(output);
        } else {
            sendError(client, "Failed to delete schedule");
        }
    } else if (action == "enable_scheduling") {
        bool enabled = doc["enabled"] | false;
        scheduleManager.setSchedulingEnabled(enabled);

        StaticJsonDocument<256> successDoc;
        successDoc["event"] = "scheduling_enabled";
        successDoc["enabled"] = enabled;
        String output;
        serializeJson(successDoc, output);
        ws.textAll(output); // Notify all clients

    // ==========================================================================
    // --- Hello Club Integration WebSocket Handlers ---
    // ==========================================================================

    } else if (action == "get_helloclub_settings") {
        // Admin only
        if (clientRole != ADMIN) {
            sendError(client, "Admin access required");
            return;
        }

        StaticJsonDocument<512> settingsDoc;
        settingsDoc["event"] = "helloclub_settings";
        settingsDoc["apiKey"] = helloClubApiKey.isEmpty() ? "" : "***configured***"; // Don't send actual key
        settingsDoc["enabled"] = helloClubEnabled;
        settingsDoc["daysAhead"] = helloClubDaysAhead;
        settingsDoc["categoryFilter"] = helloClubCategoryFilter;
        settingsDoc["syncHour"] = helloClubSyncHour;
        settingsDoc["lastSyncDay"] = lastHelloClubSyncDay;

        String output;
        serializeJson(settingsDoc, output);
        client->text(output);

    } else if (action == "save_helloclub_settings") {
        // Admin only
        if (clientRole != ADMIN) {
            sendError(client, "Admin access required");
            return;
        }

        // Update settings from request
        if (doc.containsKey("apiKey")) {
            String newApiKey = doc["apiKey"].as<String>();
            if (!newApiKey.isEmpty() && newApiKey != "***configured***") {
                helloClubApiKey = newApiKey;
            }
        }
        if (doc.containsKey("enabled")) {
            helloClubEnabled = doc["enabled"].as<bool>();
        }
        if (doc.containsKey("daysAhead")) {
            int days = doc["daysAhead"].as<int>();
            if (days >= 1 && days <= 30) {
                helloClubDaysAhead = days;
            }
        }
        if (doc.containsKey("categoryFilter")) {
            helloClubCategoryFilter = doc["categoryFilter"].as<String>();
        }
        if (doc.containsKey("syncHour")) {
            int hour = doc["syncHour"].as<int>();
            if (hour >= 0 && hour <= 23) {
                helloClubSyncHour = hour;
            }
        }

        // Save to NVS
        saveHelloClubSettings();

        StaticJsonDocument<256> successDoc;
        successDoc["event"] = "helloclub_settings_saved";
        successDoc["message"] = "Hello Club settings saved successfully";
        String output;
        serializeJson(successDoc, output);
        client->text(output);

    } else if (action == "get_helloclub_categories") {
        // Admin only
        if (clientRole != ADMIN) {
            sendError(client, "Admin access required");
            return;
        }

        std::vector<String> categories;
        if (helloClubClient.fetchAvailableCategories(helloClubDaysAhead, categories)) {
            DynamicJsonDocument categoriesDoc(1024);
            categoriesDoc["event"] = "helloclub_categories";
            JsonArray catArray = categoriesDoc.createNestedArray("categories");
            for (const auto& cat : categories) {
                catArray.add(cat);
            }
            String output;
            serializeJson(categoriesDoc, output);
            client->text(output);
        } else {
            sendError(client, "Failed to fetch categories: " + helloClubClient.getLastError());
        }

    } else if (action == "get_helloclub_events") {
        // Admin only
        if (clientRole != ADMIN) {
            sendError(client, "Admin access required");
            return;
        }

        std::vector<HelloClubEvent> events;
        if (helloClubClient.fetchEvents(helloClubDaysAhead, helloClubCategoryFilter, events)) {
            // Build response with events
            DynamicJsonDocument eventsDoc(8192); // 8KB for event list
            eventsDoc["event"] = "helloclub_events";
            JsonArray eventsArray = eventsDoc.createNestedArray("events");

            for (const auto& evt : events) {
                JsonObject eventObj = eventsArray.createNestedObject();
                eventObj["id"] = evt.id;
                eventObj["name"] = evt.name;
                eventObj["startDate"] = evt.startDate;
                eventObj["endDate"] = evt.endDate;
                eventObj["activityName"] = evt.activityName;
                eventObj["categoryName"] = evt.categoryName;
                eventObj["durationMinutes"] = evt.durationMinutes;

                // Check for conflicts
                bool hasConflict = false;
                Schedule tempSchedule;
                if (helloClubClient.convertEventToSchedule(evt, "HelloClub", tempSchedule, &myTZ)) {
                    std::vector<Schedule> existingSchedules = scheduleManager.getSchedules();
                    for (const auto& existing : existingSchedules) {
                        if (existing.dayOfWeek == tempSchedule.dayOfWeek &&
                            existing.startHour == tempSchedule.startHour &&
                            existing.startMinute == tempSchedule.startMinute) {
                            hasConflict = true;
                            eventObj["conflictWith"] = existing.clubName;
                            break;
                        }
                    }
                }
                eventObj["hasConflict"] = hasConflict;
            }

            String output;
            serializeJson(eventsDoc, output);
            client->text(output);
        } else {
            sendError(client, "Failed to fetch events: " + helloClubClient.getLastError());
        }

    } else if (action == "import_helloclub_events") {
        // Admin only
        if (clientRole != ADMIN) {
            sendError(client, "Admin access required");
            return;
        }

        JsonArray eventIds = doc["eventIds"].as<JsonArray>();
        if (eventIds.isNull() || eventIds.size() == 0) {
            sendError(client, "No events selected for import");
            return;
        }

        // Fetch all events first
        std::vector<HelloClubEvent> allEvents;
        if (!helloClubClient.fetchEvents(helloClubDaysAhead, helloClubCategoryFilter, allEvents)) {
            sendError(client, "Failed to fetch events: " + helloClubClient.getLastError());
            return;
        }

        int importedCount = 0;
        int skippedCount = 0;

        // Import only selected events
        for (JsonVariant idVariant : eventIds) {
            String selectedId = idVariant.as<String>();

            // Find the event in our fetched list
            HelloClubEvent* selectedEvent = nullptr;
            for (auto& evt : allEvents) {
                if (evt.id == selectedId) {
                    selectedEvent = &evt;
                    break;
                }
            }

            if (!selectedEvent) {
                continue;
            }

            // Convert to schedule with timezone conversion
            Schedule newSchedule;
            if (!helloClubClient.convertEventToSchedule(*selectedEvent, "HelloClub", newSchedule, &myTZ)) {
                skippedCount++;
                continue;
            }

            // Check if already exists (update) or add new
            Schedule existingSchedule;
            if (scheduleManager.getScheduleById(newSchedule.id, existingSchedule)) {
                if (scheduleManager.updateSchedule(newSchedule)) {
                    importedCount++;
                } else {
                    skippedCount++;
                }
            } else {
                if (scheduleManager.addSchedule(newSchedule)) {
                    importedCount++;
                } else {
                    skippedCount++;
                }
            }
        }

        StaticJsonDocument<256> resultDoc;
        resultDoc["event"] = "helloclub_import_complete";
        resultDoc["imported"] = importedCount;
        resultDoc["skipped"] = skippedCount;
        String output;
        serializeJson(resultDoc, output);
        client->text(output);

    } else if (action == "sync_helloclub_now") {
        // Admin only
        if (clientRole != ADMIN) {
            sendError(client, "Admin access required");
            return;
        }

        if (syncHelloClubEvents(false)) { // Manual sync, show conflict warnings
            StaticJsonDocument<256> successDoc;
            successDoc["event"] = "helloclub_sync_complete";
            successDoc["message"] = "Hello Club sync completed successfully";
            String output;
            serializeJson(successDoc, output);
            client->text(output);
        } else {
            sendError(client, "Hello Club sync failed: " + helloClubClient.getLastError());
        }
    }
    sendStateUpdate();
}


void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch(type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            authenticatedClients[client->id()] = VIEWER; // Default to viewer mode
            // Send login prompt
            StaticJsonDocument<256> loginDoc;
            loginDoc["event"] = "login_prompt";
            loginDoc["message"] = "Welcome! Login for full access or continue as viewer.";
            String output;
            serializeJson(loginDoc, output);
            client->text(output);
            // Send initial state for viewers
            sendSettingsUpdate(client);
            if (timer.getState() == RUNNING || timer.getState() == PAUSED) {
                sendSync(client);
            } else {
                sendStateUpdate(client);
            }
            // Send NTP sync status
            sendNTPStatus(client);
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            authenticatedClients.erase(client->id()); // Clean up authentication state
            authenticatedUsernames.erase(client->id()); // Clean up username mapping
            clientRateLimits.erase(client->id()); // Clean up rate limit tracking
            clientLastActivity.erase(client->id()); // Clean up session timeout tracking
            break;

        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len, client);
            break;

        case WS_EVT_ERROR:
            Serial.printf("WebSocket client #%u error\n", client->id());
            break;

        case WS_EVT_PONG:
            // Heartbeat response, ignore
            break;
    }
}

// ==========================================================================
// --- Settings Persistence ---
// ==========================================================================

// Settings are now handled by the Settings class (settings.h/cpp)

// ==========================================================================
// --- OTA Updates ---
// ==========================================================================

void setupOTA() {
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
      DEBUG_PRINTLN("Start updating " + type);
    })
    .onEnd([]() {
      DEBUG_PRINTLN("\nOTA Update complete");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      DEBUG_PRINTF("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      DEBUG_PRINTF("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) DEBUG_PRINTLN("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) DEBUG_PRINTLN("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) DEBUG_PRINTLN("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINTLN("Receive Failed");
      else if (error == OTA_END_ERROR) DEBUG_PRINTLN("End Failed");
    });

  ArduinoOTA.setHostname(MDNS_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();
  DEBUG_PRINTLN("OTA updates enabled");
}

// ==========================================================================
// --- Watchdog Timer ---
// ==========================================================================

/**
 * @brief Configure and enable the watchdog timer
 *
 * The watchdog timer will reset the ESP32 if the main loop hangs for more than
 * the configured timeout period. This provides automatic recovery from crashes.
 */
void setupWatchdog() {
    esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true); // Enable panic so ESP32 restarts
    esp_task_wdt_add(NULL); // Add current thread to WDT watch
    DEBUG_PRINTF("Watchdog timer enabled (%lu second timeout)\n", WATCHDOG_TIMEOUT_SEC);
}

// ==========================================================================
// --- Self-Test ---
// ==========================================================================

/**
 * @brief Run self-test on boot to verify hardware functionality
 *
 * Tests:
 * - SPIFFS filesystem
 * - Preferences (NVS)
 * - Relay operation
 */
void runSelfTest() {
    DEBUG_PRINTLN("Running self-test...");
    bool allTestsPassed = true;

    // Test 1: SPIFFS
    DEBUG_PRINT("  Testing SPIFFS... ");
    if (SPIFFS.begin(true)) {
        DEBUG_PRINTLN("PASS");
    } else {
        DEBUG_PRINTLN("FAIL");
        allTestsPassed = false;
    }

    // Test 2: Preferences
    DEBUG_PRINT("  Testing Preferences... ");
    Preferences prefs;
    if (prefs.begin("test", false)) {
        prefs.putString("test", "ok");
        String result = prefs.getString("test", "");
        prefs.end();
        prefs.begin("test", false);
        prefs.remove("test");
        prefs.end();
        if (result == "ok") {
            DEBUG_PRINTLN("PASS");
        } else {
            DEBUG_PRINTLN("FAIL");
            allTestsPassed = false;
        }
    } else {
        DEBUG_PRINTLN("FAIL");
        allTestsPassed = false;
    }

    // Test 3: Relay
    DEBUG_PRINT("  Testing Relay... ");
    digitalWrite(RELAY_PIN, HIGH);
    delay(100);
    digitalWrite(RELAY_PIN, LOW);
    DEBUG_PRINTLN("PASS");

    if (allTestsPassed) {
        DEBUG_PRINTLN("Self-test PASSED\n");
    } else {
        DEBUG_PRINTLN("Self-test FAILED - Some components may not function correctly\n");
    }
}

// ==========================================================================
// --- Hello Club Integration ---
// ==========================================================================

/**
 * Load Hello Club settings from NVS
 */
void loadHelloClubSettings() {
    Preferences prefs;
    if (prefs.begin("helloclub", true)) {
        helloClubApiKey = prefs.getString("apiKey", "");
        helloClubEnabled = prefs.getBool("enabled", false);
        helloClubDaysAhead = prefs.getInt("daysAhead", 7);
        helloClubCategoryFilter = prefs.getString("categoryFilter", "");
        helloClubSyncHour = prefs.getInt("syncHour", 0);
        prefs.end();

        DEBUG_PRINTLN("Hello Club settings loaded:");
        DEBUG_PRINTF("  Enabled: %s\n", helloClubEnabled ? "Yes" : "No");
        DEBUG_PRINTF("  Days Ahead: %d\n", helloClubDaysAhead);
        DEBUG_PRINTF("  Sync Hour: %d:00\n", helloClubSyncHour);
        if (!helloClubCategoryFilter.isEmpty()) {
            DEBUG_PRINTF("  Category Filter: %s\n", helloClubCategoryFilter.c_str());
        }

        // Set API key on client
        helloClubClient.setApiKey(helloClubApiKey);
    }
}

/**
 * Save Hello Club settings to NVS
 */
void saveHelloClubSettings() {
    Preferences prefs;
    if (prefs.begin("helloclub", false)) {
        prefs.putString("apiKey", helloClubApiKey);
        prefs.putBool("enabled", helloClubEnabled);
        prefs.putInt("daysAhead", helloClubDaysAhead);
        prefs.putString("categoryFilter", helloClubCategoryFilter);
        prefs.putInt("syncHour", helloClubSyncHour);
        prefs.end();

        DEBUG_PRINTLN("Hello Club settings saved");

        // Update API key on client
        helloClubClient.setApiKey(helloClubApiKey);
    }
}

/**
 * Check if it's time for daily Hello Club sync
 * Called every minute from loop()
 */
void checkDailyHelloClubSync() {
    if (!helloClubEnabled) {
        return; // Hello Club integration disabled
    }

    if (!timeStatus()) {
        return; // Wait for NTP sync
    }

    int currentHour = myTZ.hour();
    int currentDay = myTZ.day();

    // Check if we're at the configured sync hour and haven't synced today
    if (currentHour == helloClubSyncHour && lastHelloClubSyncDay != currentDay) {
        DEBUG_PRINTF("Daily Hello Club sync triggered (hour=%d, day=%d)\n", currentHour, currentDay);

        if (syncHelloClubEvents(true)) { // Auto-import, skip conflict warnings
            lastHelloClubSyncDay = currentDay;
            lastHelloClubSync = millis();
            DEBUG_PRINTLN("Hello Club sync completed successfully");
        } else {
            DEBUG_PRINTLN("Hello Club sync failed");
        }
    }
}

/**
 * Sync events from Hello Club and create schedules
 * @param skipConflictCheck If true, import even if conflicts exist (for daily sync)
 * @return true if sync successful, false otherwise
 */
bool syncHelloClubEvents(bool skipConflictCheck) {
    if (helloClubApiKey.isEmpty()) {
        Serial.println("HelloClub: API key not configured");
        return false;
    }

    // Fetch events from Hello Club
    std::vector<HelloClubEvent> events;
    if (!helloClubClient.fetchEvents(helloClubDaysAhead, helloClubCategoryFilter, events)) {
        Serial.print("HelloClub: Failed to fetch events - ");
        Serial.println(helloClubClient.getLastError());
        return false;
    }

    Serial.printf("HelloClub: Found %d events to import\n", events.size());

    int importedCount = 0;
    int skippedCount = 0;

    for (const auto& event : events) {
        // Convert event to schedule with timezone conversion
        Schedule newSchedule;
        if (!helloClubClient.convertEventToSchedule(event, "HelloClub", newSchedule, &myTZ)) {
            Serial.printf("HelloClub: Failed to convert event '%s'\n", event.name.c_str());
            skippedCount++;
            continue;
        }

        // Check for conflicts (same day/time)
        if (!skipConflictCheck) {
            bool hasConflict = false;
            std::vector<Schedule> existingSchedules = scheduleManager.getSchedules();

            for (const auto& existing : existingSchedules) {
                if (existing.dayOfWeek == newSchedule.dayOfWeek &&
                    existing.startHour == newSchedule.startHour &&
                    existing.startMinute == newSchedule.startMinute) {
                    Serial.printf("HelloClub: Skipping '%s' - conflicts with existing schedule '%s'\n",
                                  event.name.c_str(), existing.clubName.c_str());
                    hasConflict = true;
                    skippedCount++;
                    break;
                }
            }

            if (hasConflict) {
                continue;
            }
        }

        // Check if schedule with this ID already exists (update instead of duplicate)
        Schedule existingSchedule;
        if (scheduleManager.getScheduleById(newSchedule.id, existingSchedule)) {
            // Update existing Hello Club schedule
            if (scheduleManager.updateSchedule(newSchedule)) {
                Serial.printf("HelloClub: Updated schedule '%s'\n", event.name.c_str());
                importedCount++;
            } else {
                Serial.printf("HelloClub: Failed to update schedule '%s'\n", event.name.c_str());
                skippedCount++;
            }
        } else {
            // Add new schedule
            if (scheduleManager.addSchedule(newSchedule)) {
                Serial.printf("HelloClub: Imported '%s' (%s at %02d:%02d for %d min)\n",
                              event.name.c_str(),
                              newSchedule.dayOfWeek == 0 ? "Sun" :
                              newSchedule.dayOfWeek == 1 ? "Mon" :
                              newSchedule.dayOfWeek == 2 ? "Tue" :
                              newSchedule.dayOfWeek == 3 ? "Wed" :
                              newSchedule.dayOfWeek == 4 ? "Thu" :
                              newSchedule.dayOfWeek == 5 ? "Fri" : "Sat",
                              newSchedule.startHour,
                              newSchedule.startMinute,
                              newSchedule.durationMinutes);
                importedCount++;
            } else {
                Serial.printf("HelloClub: Failed to add schedule '%s'\n", event.name.c_str());
                skippedCount++;
            }
        }
    }

    Serial.printf("HelloClub: Import complete - %d imported, %d skipped\n", importedCount, skippedCount);
    return (importedCount > 0);
}
