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
#include "wifi_credentials.h" // Import the new credentials file

// ==========================================================================
// --- Hardware & Global State ---
// ==========================================================================

// Relay Pin
const int relayPin = 26;

// Timer States
enum TimerState { IDLE, RUNNING, PAUSED, FINISHED };
TimerState timerState = IDLE;

// Timer Settings (with default values)
unsigned long gameDuration = 21 * 60 * 1000;
unsigned long breakDuration = 60 * 1000;
unsigned int numRounds = 3;
bool breakTimerEnabled = true;
unsigned long sirenLength = 1000;
unsigned long sirenPause = 1000;

// Timer state variables
unsigned int currentRound = 1;
unsigned long mainTimerStart = 0;
unsigned long breakTimerStart = 0;
unsigned long mainTimerRemaining = 0;
unsigned long breakTimerRemaining = 0;
bool breakSirenSounded = false;

// Non-blocking siren state variables
bool isSirenActive = false;
int sirenBlastsRemaining = 0;
unsigned long lastSirenActionTime = 0;
bool isRelayOn = false;

// Timezone and Persistence
Timezone myTZ;
Preferences preferences;

// Web Server & WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dns;

// WebSocket Authentication
std::map<uint32_t, bool> authenticatedClients;

// Periodic Sync
unsigned long lastSyncBroadcast = 0;
const unsigned long SYNC_INTERVAL = 5000; // Sync every 5 seconds

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
void startSiren(int blasts);
void handleSiren();
void loadSettings();
void saveSettings();
void setupOTA();
String getFormattedTime12Hour();

// ==========================================================================
// --- Setup ---
// ==========================================================================

void setup() {
    Serial.begin(115200);
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW);

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed! Restarting in 5 seconds...");
        delay(5000);
        ESP.restart();
    }

    loadSettings();

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
    events(); // Process ezTime events for time synchronization
    ArduinoOTA.handle(); // Handle Over-the-Air update requests
    ws.cleanupClients(); // Clean up disconnected WebSocket clients
    handleSiren(); // Handle the non-blocking siren logic

    // Core timer logic, only runs when the timer is active
    if (timerState == RUNNING) {
        unsigned long now = millis();

        // Calculate elapsed time with overflow protection
        long mainElapsed = (long)(now - mainTimerStart);
        long breakElapsed = (long)(now - breakTimerStart);
        if (mainElapsed < 0) mainElapsed = 0; // Handle millis() overflow (~49 days)
        if (breakElapsed < 0) breakElapsed = 0;

        mainTimerRemaining = (mainElapsed < (long)gameDuration) ? (gameDuration - mainElapsed) : 0;
        breakTimerRemaining = (breakElapsed < (long)breakDuration) ? (breakDuration - breakElapsed) : 0;

        // Check if the break period has ended
        if (breakTimerEnabled && !breakSirenSounded && breakTimerRemaining == 0) {
            startSiren(1); // End of break: 1 blast
            breakSirenSounded = true;
        }

        // Check if the game round has ended
        if (mainTimerRemaining == 0) {
            startSiren(2); // End of round: 2 blasts

            if (currentRound >= numRounds) {
                timerState = FINISHED;
                sendEvent("finished");
            } else {
                currentRound++;
                mainTimerStart = millis();
                breakTimerStart = millis();
                breakSirenSounded = false;

                StaticJsonDocument<256> roundDoc;
                roundDoc["event"] = "new_round";
                roundDoc["gameDuration"] = gameDuration;
                roundDoc["breakDuration"] = breakDuration;
                roundDoc["currentRound"] = currentRound;
                String output;
                serializeJson(roundDoc, output);
                ws.textAll(output);
            }
            sendStateUpdate();
        }

        // Periodic sync broadcast to keep clients synchronized
        if (now - lastSyncBroadcast >= SYNC_INTERVAL) {
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

/**
 * @brief Handles the non-blocking siren logic.
 * This function is called on every loop and manages the on/off state of the relay
 * based on the siren settings, without using delay().
 */
void handleSiren() {
    if (!isSirenActive) {
        return;
    }

    unsigned long now = millis();

    if (isRelayOn) { // If the relay is currently on
        if (now - lastSirenActionTime >= sirenLength) {
            digitalWrite(relayPin, LOW); // Turn it off
            isRelayOn = false;
            lastSirenActionTime = now;
            sirenBlastsRemaining--;
            if (sirenBlastsRemaining <= 0) {
                isSirenActive = false; // All blasts are done
            }
        }
    } else { // If the relay is currently off
        if (sirenBlastsRemaining > 0) {
            // Pause between blasts
            if (now - lastSirenActionTime >= sirenPause) {
                digitalWrite(relayPin, HIGH); // Turn it on
                isRelayOn = true;
                lastSirenActionTime = now;
            }
        } else {
            isSirenActive = false;
        }
    }
}

/**
 * @brief Starts a non-blocking siren sequence.
 * @param blasts The number of times the siren should sound.
 */
void startSiren(int blasts) {
    if (isSirenActive) return; // Don't start a new sequence if one is running
    sirenBlastsRemaining = blasts;
    isSirenActive = true;
    isRelayOn = false;
    // Start the first blast immediately by pretending the last pause just ended
    lastSirenActionTime = millis() - sirenPause; 
}

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
    state["status"] = (timerState == RUNNING) ? "RUNNING" : ((timerState == PAUSED) ? "PAUSED" : "IDLE");
    state["mainTimer"] = (timerState == RUNNING || timerState == PAUSED) ? mainTimerRemaining : gameDuration;
    state["breakTimer"] = (timerState == RUNNING || timerState == PAUSED) ? breakTimerRemaining : breakDuration;
    state["currentRound"] = currentRound;
    state["numRounds"] = numRounds;
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
    JsonObject settings = doc.createNestedObject("settings");
    settings["gameDuration"] = gameDuration;
    settings["breakDuration"] = breakDuration;
    settings["numRounds"] = numRounds;
    settings["breakTimerEnabled"] = breakTimerEnabled;
    settings["sirenLength"] = sirenLength;
    settings["sirenPause"] = sirenPause;

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
    syncDoc["mainTimerRemaining"] = mainTimerRemaining;
    syncDoc["breakTimerRemaining"] = breakTimerRemaining;
    syncDoc["serverMillis"] = millis(); // Server timestamp for client sync
    syncDoc["currentRound"] = currentRound;
    syncDoc["numRounds"] = numRounds;
    syncDoc["status"] = (timerState == PAUSED) ? "PAUSED" : "RUNNING";

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

    // Check authentication for all actions except "authenticate"
    if (action != "authenticate") {
        if (!authenticatedClients[client->id()]) {
            sendError(client, "Authentication required. Please enter password.");
            return;
        }
    }

    // Handle authentication
    if (action == "authenticate") {
        String password = doc["password"] | "";
        if (password == WEB_PASSWORD) {
            authenticatedClients[client->id()] = true;
            StaticJsonDocument<256> authDoc;
            authDoc["event"] = "auth_success";
            authDoc["message"] = "Authentication successful";
            String output;
            serializeJson(authDoc, output);
            client->text(output);

            // Send initial state after authentication
            sendSettingsUpdate(client);
            if (timerState == RUNNING || timerState == PAUSED) {
                sendSync(client);
            } else {
                sendStateUpdate(client);
            }
        } else {
            sendError(client, "Invalid password");
        }
        return;
    }

    if (action == "start") {
        if (timerState == RUNNING || timerState == PAUSED) {
            sendError(client, "Timer already active. Reset first.");
            return;
        }
        if (timerState == IDLE || timerState == FINISHED) {
        timerState = RUNNING;
        currentRound = 1;
        mainTimerStart = millis();
        breakTimerStart = millis();
        mainTimerRemaining = gameDuration;
        breakTimerRemaining = breakDuration;
        breakSirenSounded = false;
        
        StaticJsonDocument<256> startDoc;
        startDoc["event"] = "start";
        startDoc["gameDuration"] = gameDuration;
        startDoc["breakDuration"] = breakDuration;
        startDoc["numRounds"] = numRounds;
        startDoc["currentRound"] = currentRound;
        String output;
        serializeJson(startDoc, output);
        ws.textAll(output);

    } else if (action == "pause") {
        if (timerState == RUNNING) {
            timerState = PAUSED;
            mainTimerRemaining = gameDuration > (millis() - mainTimerStart) ? gameDuration - (millis() - mainTimerStart) : 0;
            breakTimerRemaining = breakDuration > (millis() - breakTimerStart) ? breakDuration - (millis() - breakTimerStart) : 0;
            sendEvent("pause");
        } else if (timerState == PAUSED) {
            timerState = RUNNING;
            mainTimerStart = millis() - (gameDuration - mainTimerRemaining);
            breakTimerStart = millis() - (breakDuration - breakTimerRemaining);
            sendEvent("resume");
        }
    } else if (action == "reset") {
        timerState = IDLE;
        currentRound = 1;
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

        // All validation passed, save settings
        gameDuration = gameDur * 60000;
        breakDuration = breakDur * 1000;
        numRounds = rounds;
        breakTimerEnabled = settings["breakTimerEnabled"].as<bool>();
        sirenLength = sirenLen;
        sirenPause = sirenPau;
        saveSettings();
        sendSettingsUpdate(); // Notify all clients of the new settings
    }
    sendStateUpdate();
}


void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch(type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            authenticatedClients[client->id()] = false; // Require authentication
            sendAuthRequest(client); // Ask for password
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

void loadSettings() {
    if (!preferences.begin("timer", false)) {
        Serial.println("Failed to open preferences for reading. Using defaults.");
        return;
    }
    gameDuration = preferences.getULong("gameDuration", 21 * 60 * 1000);
    breakDuration = preferences.getULong("breakDuration", 60 * 1000);
    numRounds = preferences.getUInt("numRounds", 3);
    breakTimerEnabled = preferences.getBool("breakEnabled", true);
    sirenLength = preferences.getULong("sirenLength", 1000);
    sirenPause = preferences.getULong("sirenPause", 1000);
    preferences.end();
    Serial.println("Settings loaded successfully");
}

void saveSettings() {
    if (!preferences.begin("timer", false)) {
        Serial.println("Failed to open preferences for writing.");
        return;
    }
    preferences.putULong("gameDuration", gameDuration);
    preferences.putULong("breakDuration", breakDuration);
    preferences.putUInt("numRounds", numRounds);
    preferences.putBool("breakEnabled", breakTimerEnabled);
    preferences.putULong("sirenLength", sirenLength);
    preferences.putULong("sirenPause", sirenPause);
    preferences.end();
    Serial.println("Settings saved successfully");
}

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
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.setHostname("badminton-timer");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();
}
