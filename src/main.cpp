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

// ==========================================================================
// --- Function Declarations ---
// ==========================================================================

void sendEvent(const String& type);
void sendStateUpdate(AsyncWebSocketClient *client = nullptr);
void sendSettingsUpdate(AsyncWebSocketClient *client = nullptr);
void sendSync(AsyncWebSocketClient *client);
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

    // Set a static IP for the access point to make it more stable
    wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    
    // Set a timeout for the config portal
    wifiManager.setConfigPortalTimeout(180); // 3 minutes

    // Attempt to connect. If it fails, restart the ESP.
    if (!wifiManager.autoConnect("BadmintonTimerSetup")) {
        Serial.println("Failed to connect and hit timeout. Restarting...");
        delay(3000);
        ESP.restart();
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
    }
    
    // The main loop is now much simpler. It only handles the core timer logic
    // and the siren. All notifications are sent via event-based functions.
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
    if (!client) return;

    StaticJsonDocument<256> syncDoc;
    syncDoc["event"] = "sync";
    syncDoc["gameDuration"] = mainTimerRemaining;
    syncDoc["breakDuration"] = breakTimerRemaining;
    syncDoc["currentRound"] = currentRound;
    syncDoc["numRounds"] = numRounds;
    syncDoc["status"] = (timerState == PAUSED) ? "PAUSED" : "RUNNING";
    
    String output;
    serializeJson(syncDoc, output);
    client->text(output);
}

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
        gameDuration = settings["gameDuration"].as<unsigned long>() * 60000;
        breakDuration = settings["breakDuration"].as<unsigned long>() * 1000;
        numRounds = settings["numRounds"].as<unsigned int>();
        breakTimerEnabled = settings["breakTimerEnabled"].as<bool>();
        sirenLength = settings["sirenLength"].as<unsigned long>();
        sirenPause = settings["sirenPause"].as<unsigned long>();
        saveSettings();
        sendSettingsUpdate(); // Notify all clients of the new settings
    }
    sendStateUpdate();
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        sendSettingsUpdate(client);
        if (timerState == RUNNING || timerState == PAUSED) {
            sendSync(client);
        } else {
            sendStateUpdate(client);
        }
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