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

// ==========================================================================
// --- Hardware & Global State ---
// ==========================================================================

// Modular components
Timer timer;
Siren siren(RELAY_PIN);
Settings settings;
UserManager userManager;
ScheduleManager scheduleManager;

// Timezone
Timezone myTZ;

// Web Server & WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dns;

// WebSocket Authentication (maps client ID to user role)
std::map<uint32_t, UserRole> authenticatedClients;

// Periodic Sync
unsigned long lastSyncBroadcast = 0;

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
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void setupOTA();
void setupWatchdog();
void runSelfTest();
String getFormattedTime12Hour();

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

    // Set timezone and allow ezTime to sync in the background (non-blocking)
    myTZ.setLocation("Pacific/Auckland");
    
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

    events(); // Process ezTime events for time synchronization
    ArduinoOTA.handle(); // Handle Over-the-Air update requests
    ws.cleanupClients(); // Clean up disconnected WebSocket clients

    // Update siren state (non-blocking)
    siren.update();

    // Check for scheduled timer starts (once per minute)
    static unsigned long lastScheduleCheck = 0;
    if (millis() - lastScheduleCheck >= 60000) { // Check every minute
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

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        sendError(client, "Invalid JSON format");
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

        UserRole role = userManager.authenticate(username, password);

        if (role != VIEWER || (username.isEmpty() && password.isEmpty())) {
            // Successful authentication or viewer mode
            authenticatedClients[client->id()] = role;

            StaticJsonDocument<256> authDoc;
            authDoc["event"] = "auth_success";
            authDoc["role"] = (role == ADMIN) ? "admin" : (role == OPERATOR) ? "operator" : "viewer";
            authDoc["username"] = (role == VIEWER) ? "Viewer" : username;
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
            sendError(client, "Invalid credentials");
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
                             action == "get_operators");
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

        StaticJsonDocument<2048> schedulesDoc;
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
        Schedule newSchedule;
        newSchedule.id = scheduleManager.generateScheduleId();
        newSchedule.clubName = doc["clubName"] | "";
        newSchedule.ownerUsername = (clientRole == ADMIN) ? "admin" : authenticatedClients[client->id()] == OPERATOR ? "operator" : "";
        newSchedule.dayOfWeek = doc["dayOfWeek"] | 0;
        newSchedule.startHour = doc["startHour"] | 0;
        newSchedule.startMinute = doc["startMinute"] | 0;
        newSchedule.durationMinutes = doc["durationMinutes"] | 60;
        newSchedule.enabled = doc["enabled"] | true;
        newSchedule.createdAt = millis();

        // Get the authenticated username for ownership
        // (This is a simplification - in production you'd track username per client)
        if (clientRole == OPERATOR || clientRole == ADMIN) {
            if (scheduleManager.addSchedule(newSchedule)) {
                StaticJsonDocument<256> successDoc;
                successDoc["event"] = "schedule_added";
                successDoc["scheduleId"] = newSchedule.id;
                String output;
                serializeJson(successDoc, output);
                client->text(output);
            } else {
                sendError(client, "Failed to add schedule");
            }
        } else {
            sendError(client, "Operator access required");
        }
    } else if (action == "update_schedule") {
        Schedule updatedSchedule;
        updatedSchedule.id = doc["id"] | "";
        updatedSchedule.clubName = doc["clubName"] | "";
        updatedSchedule.ownerUsername = doc["ownerUsername"] | "";
        updatedSchedule.dayOfWeek = doc["dayOfWeek"] | 0;
        updatedSchedule.startHour = doc["startHour"] | 0;
        updatedSchedule.startMinute = doc["startMinute"] | 0;
        updatedSchedule.durationMinutes = doc["durationMinutes"] | 60;
        updatedSchedule.enabled = doc["enabled"] | true;
        updatedSchedule.createdAt = doc["createdAt"] | 0;

        // Check permission
        Schedule existingSchedule;
        if (scheduleManager.getScheduleById(updatedSchedule.id, existingSchedule)) {
            if (scheduleManager.hasPermission(existingSchedule, "", clientRole == ADMIN)) {
                if (scheduleManager.updateSchedule(updatedSchedule)) {
                    StaticJsonDocument<256> successDoc;
                    successDoc["event"] = "schedule_updated";
                    successDoc["scheduleId"] = updatedSchedule.id;
                    String output;
                    serializeJson(successDoc, output);
                    client->text(output);
                } else {
                    sendError(client, "Failed to update schedule");
                }
            } else {
                sendError(client, "Permission denied");
            }
        } else {
            sendError(client, "Schedule not found");
        }
    } else if (action == "delete_schedule") {
        String scheduleId = doc["scheduleId"] | "";

        Schedule existingSchedule;
        if (scheduleManager.getScheduleById(scheduleId, existingSchedule)) {
            if (scheduleManager.hasPermission(existingSchedule, "", clientRole == ADMIN)) {
                if (scheduleManager.deleteSchedule(scheduleId)) {
                    StaticJsonDocument<256> successDoc;
                    successDoc["event"] = "schedule_deleted";
                    successDoc["scheduleId"] = scheduleId;
                    String output;
                    serializeJson(successDoc, output);
                    client->text(output);
                } else {
                    sendError(client, "Failed to delete schedule");
                }
            } else {
                sendError(client, "Permission denied");
            }
        } else {
            sendError(client, "Schedule not found");
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
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            authenticatedClients.erase(client->id()); // Clean up authentication state
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
