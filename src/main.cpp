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

// Rate-limiting for WebSocket notifications
unsigned long lastNotifyTime = 0;
const unsigned long notifyInterval = 200; // Send updates every 200ms

// ==========================================================================
// --- Function Declarations ---
// ==========================================================================

void notifyClients(bool sendSettings = false);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
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
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    loadSettings();

    AsyncWiFiManager wifiManager(&server, &dns);
    wifiManager.autoConnect("BadmintonTimerSetup");

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
    dns.processNextRequest(); // Process DNS requests for the captive portal
    ArduinoOTA.handle(); // Handle Over-the-Air update requests
    ws.cleanupClients(); // Clean up disconnected WebSocket clients
    handleSiren(); // Handle the non-blocking siren logic

    // Core timer logic, only runs when the timer is active
    if (timerState == RUNNING) {
        unsigned long now = millis();
        mainTimerRemaining = gameDuration > (now - mainTimerStart) ? gameDuration - (now - mainTimerStart) : 0;
        breakTimerRemaining = breakDuration > (now - breakTimerStart) ? breakDuration - (now - breakTimerStart) : 0;

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
            } else {
                currentRound++;
                mainTimerStart = millis();
                breakTimerStart = millis();
                breakSirenSounded = false;
            }
        }
    }
    
    // Rate-limit notifications to the web UI to prevent flooding
    if (millis() - lastNotifyTime > notifyInterval) {
        notifyClients();
        lastNotifyTime = millis();
    }
}

// ==========================================================================
// --- Core Functions ---
// ==========================================================================

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

/**
 * @brief Sends the current timer state to all connected web clients.
 * @param sendSettings If true, also sends the full settings object.
 */
void notifyClients(bool sendSettings) {
    StaticJsonDocument<512> doc;
    doc["time"] = getFormattedTime12Hour();
    doc["status"] = (timerState == RUNNING) ? "RUNNING" : ((timerState == PAUSED) ? "PAUSED" : "IDLE");
    doc["mainTimer"] = (timerState == RUNNING || timerState == PAUSED) ? mainTimerRemaining : 0;
    doc["breakTimer"] = (timerState == RUNNING || timerState == PAUSED) ? breakTimerRemaining : 0;
    doc["currentRound"] = currentRound;
    doc["numRounds"] = numRounds;

    if (sendSettings) {
        JsonObject settings = doc.createNestedObject("settings");
        settings["gameDuration"] = gameDuration / 60000;
        settings["breakDuration"] = breakDuration / 1000;
        settings["numRounds"] = numRounds;
        settings["breakTimerEnabled"] = breakTimerEnabled;
        settings["sirenLength"] = sirenLength / 1000;
        settings["sirenPause"] = sirenPause / 1000;
    }

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

/**
 * @brief Handles incoming messages from a WebSocket client.
 */
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }

    String action = doc["action"];

    if (action == "start" && (timerState == IDLE || timerState == FINISHED)) {
        timerState = RUNNING;
        currentRound = 1;
        mainTimerStart = millis();
        breakTimerStart = millis();
        mainTimerRemaining = gameDuration;
        breakTimerRemaining = breakDuration;
        breakSirenSounded = false;
    } else if (action == "pause") {
        if (timerState == RUNNING) {
            timerState = PAUSED;
        } else if (timerState == PAUSED) {
            timerState = RUNNING;
            // To resume, we adjust the start times to account for the time that was paused.
            mainTimerStart = millis() - (gameDuration - mainTimerRemaining);
            breakTimerStart = millis() - (breakDuration - breakTimerRemaining);
        }
    } else if (action == "reset") {
        timerState = IDLE;
        currentRound = 1;
    } else if (action == "save_settings") {
        JsonObject settings = doc["settings"];
        gameDuration = settings["gameDuration"].as<unsigned long>() * 60000;
        breakDuration = settings["breakDuration"].as<unsigned long>() * 1000;
        numRounds = settings["numRounds"].as<unsigned int>();
        breakTimerEnabled = settings["breakTimerEnabled"].as<bool>();
        sirenLength = settings["sirenLength"].as<unsigned long>() * 1000;
        sirenPause = settings["sirenPause"].as<unsigned long>() * 1000;
        saveSettings();
    }
    notifyClients(true); // Send an immediate update after an action
}

/**
 * @brief Event handler for WebSocket connections.
 */
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        notifyClients(true); // Send settings on new connection
    } else if (type == WS_EVT_DATA) {
        handleWebSocketMessage(arg, data, len);
    }
}

// ==========================================================================
// --- Settings Persistence ---
// ==========================================================================

void loadSettings() {
    preferences.begin("timer", false);
    gameDuration = preferences.getULong("gameDuration", 15 * 60 * 1000);
    breakDuration = preferences.getULong("breakDuration", 60 * 1000);
    numRounds = preferences.getUInt("numRounds", 4);
    breakTimerEnabled = preferences.getBool("breakEnabled", false);
    sirenLength = preferences.getULong("sirenLength", 1000);
    sirenPause = preferences.getULong("sirenPause", 1000);
    preferences.end();
}

void saveSettings() {
    preferences.begin("timer", false);
    preferences.putULong("gameDuration", gameDuration);
    preferences.putULong("breakDuration", breakDuration);
    preferences.putUInt("numRounds", numRounds);
    preferences.putBool("breakEnabled", breakTimerEnabled);
    preferences.putULong("sirenLength", sirenLength);
    preferences.putULong("sirenPause", sirenPause);
    preferences.end();
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
  ArduinoOTA.setPassword("badminton");
  ArduinoOTA.begin();
}