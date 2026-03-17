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
#include "helloclub.h"
#include "esp_system.h"

// ==========================================================================
// --- Boot Logger (persists to SPIFFS across reboots) ---
// ==========================================================================

#define BOOT_LOG_PATH "/bootlog.txt"
#define BOOT_LOG_MAX_SIZE 8192

void bootLogInit() {
    File f = SPIFFS.open(BOOT_LOG_PATH, "r");
    if (f && f.size() > BOOT_LOG_MAX_SIZE) {
        String content = f.readString();
        f.close();
        int cutPoint = content.indexOf('\n', content.length() / 2);
        if (cutPoint > 0) {
            File fw = SPIFFS.open(BOOT_LOG_PATH, "w");
            if (fw) {
                fw.print("--- LOG TRIMMED ---\n");
                fw.print(content.substring(cutPoint + 1));
                fw.close();
            }
        }
    } else if (f) {
        f.close();
    }
}

void bootLog(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Serial.print("[LOG] ");
    Serial.println(buf);

    File f = SPIFFS.open(BOOT_LOG_PATH, "a");
    if (f) {
        f.printf("[%lu ms] %s\n", millis(), buf);
        f.close();
    }
}

const char* getResetReasonStr() {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON:   return "POWER_ON";
        case ESP_RST_EXT:       return "EXTERNAL";
        case ESP_RST_SW:        return "SOFTWARE";
        case ESP_RST_PANIC:     return "PANIC/CRASH";
        case ESP_RST_INT_WDT:   return "INTERRUPT_WATCHDOG";
        case ESP_RST_TASK_WDT:  return "TASK_WATCHDOG";
        case ESP_RST_WDT:       return "OTHER_WATCHDOG";
        case ESP_RST_DEEPSLEEP: return "DEEP_SLEEP";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "UNKNOWN";
    }
}

// ==========================================================================
// --- Hardware & Global State ---
// ==========================================================================

Timer timer;
Siren siren(RELAY_PIN);
Settings settings;
UserManager userManager;
HelloClubClient helloClubClient;

// Timezone
Timezone myTZ;

// Web Server & WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dns;

// WebSocket Authentication
std::map<uint32_t, UserRole> authenticatedClients;
std::map<uint32_t, String> authenticatedUsernames;

// Rate Limiting
struct RateLimitInfo {
    unsigned long windowStart;
    int messageCount;
};
std::map<uint32_t, RateLimitInfo> clientRateLimits;
const unsigned long RATE_LIMIT_WINDOW = 1000;

// Session Timeout
std::map<uint32_t, unsigned long> clientLastActivity;
const unsigned long SESSION_TIMEOUT = 30 * 60 * 1000;

// Periodic Sync
unsigned long lastSyncBroadcast = 0;

// NTP Sync Status Tracking
bool lastNTPSyncStatus = false;
unsigned long lastNTPStatusCheck = 0;
const unsigned long NTP_CHECK_INTERVAL = 5000;

// Factory Reset Button State
unsigned long factoryResetButtonPressStart = 0;
bool factoryResetButtonPressed = false;
bool factoryResetInProgress = false;

// Hello Club Integration
String helloClubApiKey = "";
bool helloClubEnabled = false;
unsigned long lastHelloClubPoll = 0;
bool lastHelloClubPollFailed = false;

// Background fetch task (FreeRTOS)
volatile bool hcFetchInProgress = false;
volatile bool hcFetchResultReady = false;
volatile bool hcFetchResultSuccess = false;
TaskHandle_t hcFetchTaskHandle = nullptr;

// Event Window Enforcement
time_t activeEventEndTime = 0;
String activeEventName = "";
String activeEventId = "";

// Boot Recovery
bool bootRecoveryAttempted = false;

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
void sendUpcomingEvents(AsyncWebSocketClient *client = nullptr);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void setupOTA();
void setupWatchdog();
void runSelfTest();
String getFormattedTime12Hour();
void loadHelloClubSettings();
void saveHelloClubSettings();
void checkHelloClubPoll();
bool sirenAllowed();

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

    pinMode(FACTORY_RESET_BUTTON_PIN, INPUT_PULLUP);
    DEBUG_PRINTLN("Factory reset button configured (hold BOOT button for 10 seconds)");

    if (ENABLE_SELF_TEST) {
        runSelfTest();
    }

    if (!SPIFFS.begin(true)) {
        DEBUG_PRINTLN("SPIFFS mount failed! Restarting in 5 seconds...");
        delay(SPIFFS_RESTART_DELAY_MS);
        ESP.restart();
    }

    bootLogInit();
    bootLog("===== BOOT =====");
    bootLog("Firmware: v%s (%s %s)", FIRMWARE_VERSION, BUILD_DATE, BUILD_TIME);
    bootLog("Reset reason: %s", getResetReasonStr());
    bootLog("Free heap: %u bytes", ESP.getFreeHeap());

    siren.begin();
    settings.load(timer, siren);
    userManager.begin();
    loadHelloClubSettings();

    // Set HC client defaults from settings and load cached events
    helloClubClient.setDefaults(settings.getHcDefaultDuration(), DEFAULT_NUM_ROUNDS);
    helloClubClient.loadFromNVS();

    bootLog("WiFi: Starting connection attempts");

    if (!connectToKnownWiFi()) {
        bootLog("WiFi: Known networks failed, starting custom captive portal");

        bootLog("Portal: Scanning networks before starting AP...");
        int numNetworks = WiFi.scanNetworks();
        int displayCount = min(numNetworks, 10); // Cap at 10 to limit RAM usage
        String networkListHtml = "";
        networkListHtml.reserve(displayCount * 100);
        for (int i = 0; i < numNetworks; i++) {
            String ssid = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);
            bool isOpen = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
            if (i < displayCount) {
                networkListHtml += "<div class='net' onclick=\"document.getElementById('s').value='" + ssid + "'\">";
                networkListHtml += ssid + " (" + String(rssi) + " dBm)";
                if (isOpen) networkListHtml += " [open]";
                networkListHtml += "</div>";
            }
            bootLog("  Found: '%s' RSSI:%d %s", ssid.c_str(), rssi, isOpen ? "OPEN" : "ENCRYPTED");
        }
        WiFi.scanDelete();

        WiFi.mode(WIFI_AP);
        delay(100);
        WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        WiFi.softAP("BadmintonTimerSetup");
        bootLog("Portal: AP started, IP: %s", WiFi.softAPIP().toString().c_str());

        dns.start(53, "*", IPAddress(192, 168, 4, 1));
        bootLog("Portal: DNS captive redirect active");

        String configPage = R"rawhtml(
<!DOCTYPE html><html><head>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Badminton Timer WiFi Setup</title>
<style>
body{font-family:sans-serif;margin:20px;background:#1a1a2e;color:#eee}
h2{color:#e94560}
.net{padding:10px;margin:5px 0;background:#16213e;border-radius:5px;cursor:pointer}
.net:hover{background:#0f3460}
input{width:100%;padding:10px;margin:5px 0;box-sizing:border-box;border-radius:5px;border:1px solid #555;background:#16213e;color:#eee;font-size:16px}
button{width:100%;padding:12px;background:#e94560;color:#fff;border:none;border-radius:5px;font-size:18px;cursor:pointer;margin-top:10px}
button:hover{background:#c73e54}
.status{padding:10px;margin:10px 0;background:#16213e;border-radius:5px}
</style></head><body>
<h2>Badminton Timer WiFi Setup</h2>
<div class='status'>Select a network or type the SSID manually</div>
)rawhtml";
        configPage += networkListHtml;
        configPage += R"rawhtml(
<form action='/save' method='POST'>
<label>SSID:</label><input id='s' name='s' required>
<label>Password (leave blank for open):</label><input name='p' type='password'>
<button type='submit'>Connect</button>
</form></body></html>
)rawhtml";

        server.on("/", HTTP_GET, [configPage](AsyncWebServerRequest *request){
            bootLog("Portal: Config page served to client");
            request->send(200, "text/html", configPage);
        });

        server.onNotFound([](AsyncWebServerRequest *request){
            bootLog("Portal: Redirecting %s -> /", request->url().c_str());
            request->redirect("http://192.168.4.1/");
        });

        volatile bool portalDone = false;
        String savedSSID, savedPassword;

        server.on("/save", HTTP_POST, [&portalDone, &savedSSID, &savedPassword](AsyncWebServerRequest *request){
            if (request->hasParam("s", true)) {
                savedSSID = request->getParam("s", true)->value();
                savedPassword = request->hasParam("p", true) ? request->getParam("p", true)->value() : "";
                bootLog("Portal: User submitted SSID='%s'", savedSSID.c_str());
                request->send(200, "text/html",
                    "<html><body style='font-family:sans-serif;background:#1a1a2e;color:#eee;text-align:center;padding:40px'>"
                    "<h2>Connecting...</h2><p>The device will restart and connect to " + savedSSID + "</p>"
                    "<p>If it fails, the setup portal will reappear.</p></body></html>");
                portalDone = true;
            } else {
                request->send(400, "text/plain", "Missing SSID");
            }
        });

        server.begin();
        bootLog("Portal: Web server started, waiting for user...");

        unsigned long portalStart = millis();
        while (!portalDone && millis() - portalStart < 300000) {
            dns.processNextRequest();
            esp_task_wdt_reset();
            delay(10);
        }

        server.end();
        dns.stop();

        if (portalDone && savedSSID.length() > 0) {
            bootLog("Portal: Trying to connect to '%s'...", savedSSID.c_str());
            WiFi.mode(WIFI_STA);
            delay(200);
            if (savedPassword.length() > 0) {
                WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
            } else {
                WiFi.begin(savedSSID.c_str());
            }

            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
                esp_task_wdt_reset();
                delay(500);
                Serial.print(".");
            }
            Serial.println();

            if (WiFi.status() == WL_CONNECTED) {
                bootLog("Portal: Connected to '%s' IP: %s", savedSSID.c_str(), WiFi.localIP().toString().c_str());
            } else {
                bootLog("Portal: Failed to connect to '%s', restarting", savedSSID.c_str());
                delay(2000);
                ESP.restart();
            }
        } else {
            bootLog("Portal: TIMEOUT after 5 min, restarting");
            delay(2000);
            ESP.restart();
        }
    }

    // Captive portal success responses
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(204); });
    server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(204); });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Microsoft Connect Test");
    });
    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Microsoft NCSI");
    });

    bootLog("WiFi: CONNECTED to '%s' IP: %s RSSI: %d dBm",
        WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());

    Serial.println("Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    if (ENABLE_WATCHDOG) {
        setupWatchdog();
        bootLog("Watchdog: Enabled (%lu sec timeout)", WATCHDOG_TIMEOUT_SEC);
    }

    setupOTA();

    if (!MDNS.begin("badminton-timer")) {
        Serial.println("Error setting up MDNS responder!");
    }
    MDNS.addService("http", "tcp", 80);

    server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
        if (SPIFFS.exists(BOOT_LOG_PATH)) {
            request->send(SPIFFS, BOOT_LOG_PATH, "text/plain");
        } else {
            request->send(200, "text/plain", "No boot log found.");
        }
    });

    server.on("/log/clear", HTTP_GET, [](AsyncWebServerRequest *request){
        SPIFFS.remove(BOOT_LOG_PATH);
        request->send(200, "text/plain", "Boot log cleared.");
    });

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(204); });

    String configuredTimezone = settings.getTimezone();
    myTZ.setLocation(configuredTimezone);
    Serial.printf("Timezone configured: %s\n", configuredTimezone.c_str());

    // Throttle NTP polling — default ezTime polls every 30 min which can
    // block the main loop during UDP round-trips. Set to 60 min.
    setInterval(3600);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html", "text/html");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });
    server.serveStatic("/", SPIFFS, "/").setCacheControl("no-cache");

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();
}

// ==========================================================================
// --- Main Loop ---
// ==========================================================================

void loop() {
    if (ENABLE_WATCHDOG) {
        esp_task_wdt_reset();
    }

    // Factory reset button
    if (!factoryResetInProgress) {
        bool buttonCurrentlyPressed = (digitalRead(FACTORY_RESET_BUTTON_PIN) == LOW);

        if (buttonCurrentlyPressed && !factoryResetButtonPressed) {
            factoryResetButtonPressStart = millis();
            factoryResetButtonPressed = true;
            DEBUG_PRINTLN("Factory reset button pressed - hold for 10 seconds...");
        } else if (buttonCurrentlyPressed && factoryResetButtonPressed) {
            unsigned long holdDuration = millis() - factoryResetButtonPressStart;

            static unsigned long lastFeedback = 0;
            if (holdDuration - lastFeedback >= 2000) {
                lastFeedback = holdDuration;
                DEBUG_PRINTF("Factory reset: %lu seconds...\n", holdDuration / 1000);
                digitalWrite(RELAY_PIN, HIGH);
                delay(100);
                digitalWrite(RELAY_PIN, LOW);
            }

            if (holdDuration >= FACTORY_RESET_HOLD_TIME_MS) {
                factoryResetInProgress = true;
                DEBUG_PRINTLN("\n=================================");
                DEBUG_PRINTLN("FACTORY RESET TRIGGERED!");
                DEBUG_PRINTLN("=================================\n");

                for (int i = 0; i < 5; i++) {
                    digitalWrite(RELAY_PIN, HIGH);
                    delay(200);
                    digitalWrite(RELAY_PIN, LOW);
                    delay(200);
                }

                userManager.factoryReset();

                timer.setGameDuration(DEFAULT_GAME_DURATION);
                timer.setNumRounds(DEFAULT_NUM_ROUNDS);
                siren.setBlastLength(DEFAULT_SIREN_LENGTH);
                siren.setBlastPause(DEFAULT_SIREN_PAUSE);
                settings.save(timer, siren);
                timer.reset();

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
            unsigned long holdDuration = millis() - factoryResetButtonPressStart;
            DEBUG_PRINTF("Factory reset cancelled (held for %lu ms)\n", holdDuration);
            factoryResetButtonPressed = false;
            factoryResetButtonPressStart = 0;
        }
    }

    // Heap monitoring — log every 5 minutes, warn if low
    static unsigned long lastHeapLog = 0;
    if (millis() - lastHeapLog >= 300000) {
        lastHeapLog = millis();
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t minFreeHeap = ESP.getMinFreeHeap();
        DEBUG_PRINTF("Heap: %u free, %u min free (since boot)\n", freeHeap, minFreeHeap);
        if (freeHeap < 10240) {
            DEBUG_PRINTLN("WARNING: Free heap below 10KB!");
            bootLog("HEAP WARNING: %u bytes free", freeHeap);
        }
    }

    // WiFi reconnection monitoring — check every 30 seconds
    static unsigned long lastWiFiCheck = 0;
    static unsigned long wifiDownSince = 0;
    if (millis() - lastWiFiCheck >= 30000) {
        lastWiFiCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
            if (wifiDownSince == 0) {
                wifiDownSince = millis();
                DEBUG_PRINTLN("WiFi: Connection lost, waiting for auto-reconnect...");
                bootLog("WiFi: Connection lost");
            }
            unsigned long downTime = millis() - wifiDownSince;
            // If auto-reconnect hasn't worked after 2 minutes, force a reconnect
            if (downTime > 120000) {
                DEBUG_PRINTLN("WiFi: Auto-reconnect failed, forcing reconnect...");
                bootLog("WiFi: Forcing reconnect after %lu sec", downTime / 1000);
                WiFi.disconnect(true);
                delay(200);
                WiFi.reconnect();
                wifiDownSince = millis(); // Reset timer for next attempt
            }
        } else if (wifiDownSince > 0) {
            unsigned long downTime = millis() - wifiDownSince;
            DEBUG_PRINTF("WiFi: Reconnected after %lu seconds\n", downTime / 1000);
            bootLog("WiFi: Reconnected after %lu sec", downTime / 1000);
            wifiDownSince = 0;
        }
    }

    events(); // ezTime
    ArduinoOTA.handle();
    ws.cleanupClients();

    checkAndBroadcastNTPStatus();
    siren.update();

    // Boot recovery: if we rebooted mid-event, resume the timer
    if (!bootRecoveryAttempted && helloClubEnabled && lastNTPSyncStatus) {
        bootRecoveryAttempted = true;

        // Check if operator manually cancelled before the reboot
        String cancelledId = "";
        {
            Preferences cancelPrefs;
            if (cancelPrefs.begin("helloclub", true)) {
                cancelledId = cancelPrefs.getString("evt_cancel", "");
                cancelPrefs.end();
            }
        }

        // Also check if we already recovered this event (prevents reboot→recover→crash loops)
        String lastRecoveredId = "";
        {
            Preferences recPrefs;
            if (recPrefs.begin("helloclub", true)) {
                lastRecoveredId = recPrefs.getString("last_recov", "");
                recPrefs.end();
            }
        }

        RecoveryResult recovery = helloClubClient.checkMidEventRecovery();
        if (recovery.shouldRecover && recovery.eventId != cancelledId && recovery.eventId != lastRecoveredId) {
            // Persist this event ID immediately so if we crash during recovery, we won't retry
            {
                Preferences recPrefs;
                if (recPrefs.begin("helloclub", false)) {
                    recPrefs.putString("last_recov", recovery.eventId);
                    recPrefs.end();
                }
            }

            DEBUG_PRINTF("Boot recovery: %s round %u, %lu ms remaining\n",
                         recovery.eventName.c_str(), recovery.currentRound, recovery.remainingMs);

            timer.setGameDuration(recovery.durationMin * 60000UL);
            if (recovery.numRounds > 0) {
                timer.setNumRounds(recovery.numRounds);
                timer.setContinuousMode(false);
            } else {
                timer.setContinuousMode(true);
            }
            timer.startMidRound(recovery.currentRound, recovery.remainingMs);

            activeEventEndTime = recovery.eventEndTime;
            activeEventName = recovery.eventName;
            activeEventId = recovery.eventId;

            // Broadcast recovery notification
            StaticJsonDocument<512> recDoc;
            recDoc["event"] = "event_auto_resumed";
            recDoc["eventName"] = recovery.eventName;
            recDoc["durationMin"] = recovery.durationMin;
            recDoc["currentRound"] = recovery.currentRound;
            recDoc["remainingMs"] = recovery.remainingMs;
            recDoc["eventEndTime"] = (long)recovery.eventEndTime;
            String output;
            serializeJson(recDoc, output);
            ws.textAll(output);

            sendStateUpdate();
            bootLog("Boot recovery: resumed %s round %u", recovery.eventName.c_str(), recovery.currentRound);
        } else if (recovery.shouldRecover && recovery.eventId == lastRecoveredId) {
            DEBUG_PRINTF("Boot recovery: skipped %s (already recovered, preventing loop)\n", recovery.eventId.c_str());
            bootLog("Boot recovery: skipped %s (already recovered)", recovery.eventId.c_str());
        } else if (recovery.shouldRecover) {
            DEBUG_PRINTF("Boot recovery: skipped %s (manually cancelled)\n", recovery.eventId.c_str());
            bootLog("Boot recovery: skipped %s (cancelled)", recovery.eventId.c_str());
        }
    }

    // Session timeout check (every 60 seconds)
    static unsigned long lastSessionCheck = 0;
    if (millis() - lastSessionCheck >= 60000) {
        lastSessionCheck = millis();
        unsigned long now = millis();

        for (auto it = clientLastActivity.begin(); it != clientLastActivity.end(); ) {
            uint32_t clientId = it->first;
            unsigned long lastActivity = it->second;

            if (now - lastActivity >= SESSION_TIMEOUT) {
                if (authenticatedClients.find(clientId) != authenticatedClients.end() &&
                    authenticatedClients[clientId] != VIEWER) {

                    Serial.printf("Session timeout for client #%u\n", clientId);
                    authenticatedClients[clientId] = VIEWER;
                    authenticatedUsernames.erase(clientId);

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

    // Hello Club polling (every 30 seconds check if it's time to poll)
    static unsigned long lastHCCheck = 0;
    if (millis() - lastHCCheck >= SCHEDULE_CHECK_INTERVAL_MS) {
        lastHCCheck = millis();
        checkHelloClubPoll();

        // Auto-trigger check
        if (helloClubEnabled) {
            helloClubClient.purgeExpired(myTZ);

            CachedEvent* evt = helloClubClient.checkAutoTrigger(myTZ);
            if (evt && (timer.getState() == IDLE || timer.getState() == FINISHED)) {
                DEBUG_PRINTF("Auto-trigger: %s\n", evt->name.c_str());

                timer.setGameDuration(evt->durationMin * 60000UL);
                if (evt->numRounds > 0) {
                    timer.setNumRounds(evt->numRounds);
                    timer.setContinuousMode(false);
                } else {
                    timer.setContinuousMode(true);
                }
                timer.start();

                helloClubClient.markTriggered(evt->id);

                // Set event window
                activeEventEndTime = evt->endTime;
                activeEventName = evt->name;
                activeEventId = evt->id;

                // Clear any stale cancel flag (new event starting)
                {
                    Preferences cancelPrefs;
                    if (cancelPrefs.begin("helloclub", false)) {
                        cancelPrefs.remove("evt_cancel");
                        cancelPrefs.end();
                    }
                }

                // Broadcast auto-start notification
                StaticJsonDocument<512> startDoc;
                startDoc["event"] = "event_auto_started";
                startDoc["eventName"] = evt->name;
                startDoc["durationMin"] = evt->durationMin;
                startDoc["eventEndTime"] = (long)evt->endTime;
                String output;
                serializeJson(startDoc, output);
                ws.textAll(output);

                sendStateUpdate();
                DEBUG_PRINTLN("Timer auto-started by Hello Club event");
            }
        }
    }

    // Event window enforcement — hard cutoff
    if (activeEventEndTime > 0 &&
        (timer.getState() == RUNNING || timer.getState() == PAUSED)) {
        time_t now = UTC.now();
        if (now >= activeEventEndTime) {
            timer.reset();
            siren.stop();

            StaticJsonDocument<256> cutoffDoc;
            cutoffDoc["event"] = "event_cutoff";
            cutoffDoc["message"] = "Session ended - booking time expired";
            cutoffDoc["eventName"] = activeEventName;
            String output;
            serializeJson(cutoffDoc, output);
            ws.textAll(output);

            DEBUG_PRINTF("Event cutoff: %s\n", activeEventName.c_str());
            activeEventEndTime = 0;
            activeEventName = "";
            activeEventId = "";

            sendStateUpdate();
        }
    }

    // Update timer state
    if (timer.update()) {
        if (timer.hasRoundEnded()) {
            if (timer.isMatchFinished()) {
                if (sirenAllowed()) siren.start(3);
                sendEvent("finished");
                DEBUG_PRINTLN("Match completed! All rounds finished.");
            } else {
                // Round ended — siren fires
                if (sirenAllowed()) siren.start(2);

                if (timer.getState() == PAUSED) {
                    // pauseAfterNext triggered — tell clients we're paused
                    StaticJsonDocument<256> pauseDoc;
                    pauseDoc["event"] = "pause";
                    pauseDoc["mainTimerRemaining"] = timer.getMainTimerRemaining();
                    pauseDoc["currentRound"] = timer.getCurrentRound();
                    pauseDoc["numRounds"] = timer.getNumRounds();
                    String output;
                    serializeJson(pauseDoc, output);
                    ws.textAll(output);
                } else {
                    // Normal next round
                    StaticJsonDocument<256> roundDoc;
                    roundDoc["event"] = "new_round";
                    roundDoc["gameDuration"] = timer.getGameDuration();
                    roundDoc["currentRound"] = timer.getCurrentRound();
                    roundDoc["numRounds"] = timer.getNumRounds();
                    roundDoc["pauseAfterNext"] = timer.getPauseAfterNext();
                    roundDoc["continuousMode"] = timer.getContinuousMode();
                    String output;
                    serializeJson(roundDoc, output);
                    ws.textAll(output);
                }
            }
            sendStateUpdate();
        }
    }

    // Periodic sync broadcast
    if (timer.getState() == RUNNING) {
        unsigned long now = millis();
        if (now - lastSyncBroadcast >= SYNC_INTERVAL_MS) {
            sendSync(nullptr);
            lastSyncBroadcast = now;
        }
    }
}

// ==========================================================================
// --- Core Functions ---
// ==========================================================================

bool connectToKnownWiFi() {
    Serial.println("Trying to connect to a known WiFi network...");
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    bootLog("WiFi: Scanning for networks...");
    int numFound = WiFi.scanNetworks();
    bootLog("WiFi: Scan found %d networks", numFound);
    for (int i = 0; i < numFound; i++) {
        bootLog("  %d: '%s' RSSI:%d Ch:%d %s", i + 1,
            WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
            WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "ENCRYPTED");
    }
    WiFi.scanDelete();

    for (size_t i = 0; i < known_networks_count; i++) {
        const WiFiCredential& cred = known_networks[i];
        bootLog("WiFi: Trying '%s' (attempt 1/1)", cred.ssid);

        WiFi.disconnect(true);
        delay(200);

        if (strlen(cred.password) > 0) {
            WiFi.begin(cred.ssid, cred.password);
        } else {
            WiFi.begin(cred.ssid);
        }

        unsigned long startAttemptTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
            esp_task_wdt_reset();
            delay(500);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            bootLog("WiFi: Connected to '%s' IP: %s", cred.ssid, WiFi.localIP().toString().c_str());
            return true;
        } else {
            int status = WiFi.status();
            const char* statusStr;
            switch(status) {
                case WL_NO_SSID_AVAIL: statusStr = "NO_SSID_AVAIL"; break;
                case WL_CONNECT_FAILED: statusStr = "CONNECT_FAILED"; break;
                case WL_CONNECTION_LOST: statusStr = "CONNECTION_LOST"; break;
                case WL_DISCONNECTED: statusStr = "DISCONNECTED"; break;
                case WL_IDLE_STATUS: statusStr = "IDLE"; break;
                default: statusStr = "UNKNOWN"; break;
            }
            bootLog("WiFi: '%s' FAILED - status: %d (%s)", cred.ssid, status, statusStr);
            WiFi.disconnect(true);
        }
    }
    return false;
}

// ==========================================================================
// --- Helper Functions ---
// ==========================================================================

String getFormattedTime12Hour() {
    return myTZ.dateTime("h:i:s a");
}

bool sirenAllowed() {
    if (activeEventEndTime == 0) return true;
    time_t now = UTC.now();
    return now < activeEventEndTime;
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
    state["currentRound"] = timer.getCurrentRound();
    state["numRounds"] = timer.getNumRounds();
    state["time"] = getFormattedTime12Hour();
    state["pauseAfterNext"] = timer.getPauseAfterNext();
    state["continuousMode"] = timer.getContinuousMode();

    // Include event window info
    if (activeEventEndTime > 0) {
        state["activeEventEndTime"] = (long)activeEventEndTime;
        state["activeEventName"] = activeEventName;
    }

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
    settingsObj["numRounds"] = timer.getNumRounds();
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
    syncDoc["serverMillis"] = millis();
    syncDoc["currentRound"] = timer.getCurrentRound();
    syncDoc["numRounds"] = timer.getNumRounds();
    syncDoc["status"] = (timer.getState() == PAUSED) ? "PAUSED" : "RUNNING";
    syncDoc["pauseAfterNext"] = timer.getPauseAfterNext();
    syncDoc["continuousMode"] = timer.getContinuousMode();

    if (activeEventEndTime > 0) {
        syncDoc["activeEventEndTime"] = (long)activeEventEndTime;
    }

    String output;
    serializeJson(syncDoc, output);

    if (client) {
        client->text(output);
    } else {
        ws.textAll(output);
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

    bool synced = (myTZ.year() > 2020 && myTZ.year() < 2100 &&
                   myTZ.month() >= 1 && myTZ.month() <= 12 &&
                   myTZ.day() >= 1 && myTZ.day() <= 31);

    doc["synced"] = synced;
    doc["time"] = synced ? getFormattedTime12Hour() : "Not synced";

    if (synced) {
        doc["timezone"] = settings.getTimezone();
        doc["dateTime"] = myTZ.dateTime("Y-m-d H:i:s");
        doc["autoSyncInterval"] = 30;
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

    if (now - lastNTPStatusCheck < NTP_CHECK_INTERVAL) {
        return;
    }

    lastNTPStatusCheck = now;

    bool currentSyncStatus = (myTZ.year() > 2020 && myTZ.year() < 2100 &&
                              myTZ.month() >= 1 && myTZ.month() <= 12 &&
                              myTZ.day() >= 1 && myTZ.day() <= 31);

    if (currentSyncStatus != lastNTPSyncStatus) {
        lastNTPSyncStatus = currentSyncStatus;
        sendNTPStatus(nullptr);
    }
}

void sendUpcomingEvents(AsyncWebSocketClient *client) {
    const auto& events = helloClubClient.getCachedEvents();

    DynamicJsonDocument doc(4096);
    doc["event"] = "upcoming_events";
    doc["lastSync"] = helloClubClient.getLastSyncTime();
    doc["enabled"] = helloClubEnabled;

    JsonArray eventsArr = doc.createNestedArray("events");
    for (const auto& evt : events) {
        JsonObject obj = eventsArr.createNestedObject();
        obj["id"] = evt.id;
        obj["name"] = evt.name;
        obj["startTime"] = (long)evt.startTime;
        obj["endTime"] = (long)evt.endTime;
        obj["durationMin"] = evt.durationMin;
        obj["numRounds"] = evt.numRounds;
        obj["triggered"] = evt.triggered;
    }

    String output;
    serializeJson(doc, output);

    if (client) {
        client->text(output);
    } else {
        ws.textAll(output);
    }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client) {
    // Rate limiting
    unsigned long now = millis();
    uint32_t clientId = client->id();

    if (clientRateLimits.find(clientId) == clientRateLimits.end()) {
        clientRateLimits[clientId] = {now, 0};
    }

    RateLimitInfo& rateLimit = clientRateLimits[clientId];

    if (now - rateLimit.windowStart >= RATE_LIMIT_WINDOW) {
        rateLimit.windowStart = now;
        rateLimit.messageCount = 0;
    }

    rateLimit.messageCount++;

    if (rateLimit.messageCount > MAX_MESSAGES_PER_SECOND) {
        Serial.printf("Rate limit exceeded for client #%u (%d msgs/sec)\n", clientId, rateLimit.messageCount);
        sendError(client, "ERR_RATE_LIMIT: Too many requests. Please slow down.");
        return;
    }

    clientLastActivity[clientId] = now;

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        Serial.println(F("deserializeJson() failed"));
        sendError(client, "ERR_INVALID_JSON: Invalid message format");
        return;
    }

    String action = doc["action"];

    UserRole clientRole = VIEWER;
    if (authenticatedClients.find(client->id()) != authenticatedClients.end()) {
        clientRole = authenticatedClients[client->id()];
    }

    // Handle authentication
    if (action == "authenticate") {
        String username = doc["username"] | "";
        String password = doc["password"] | "";

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

            sendSettingsUpdate(client);
            if (timer.getState() == RUNNING || timer.getState() == PAUSED) {
                sendSync(client);
            } else {
                sendStateUpdate(client);
            }
            return;
        }

        UserRole role = userManager.authenticate(username, password);

        if (role != VIEWER) {
            authenticatedClients[client->id()] = role;
            authenticatedUsernames[client->id()] = username;

            StaticJsonDocument<256> authDoc;
            authDoc["event"] = "auth_success";
            authDoc["role"] = (role == ADMIN) ? "admin" : "operator";
            authDoc["username"] = username;
            String output;
            serializeJson(authDoc, output);
            client->text(output);

            sendSettingsUpdate(client);
            if (timer.getState() == RUNNING || timer.getState() == PAUSED) {
                sendSync(client);
            } else {
                sendStateUpdate(client);
            }
        } else {
            sendError(client, "ERR_AUTH_FAILED: Invalid username or password");
        }
        return;
    }

    // Permission checks
    const bool needsOperator = (action == "start" || action == "pause" || action == "reset" ||
                                action == "pause_after_next" ||
                                action == "helloclub_refresh");
    if (needsOperator && clientRole < OPERATOR) {
        sendError(client, "Operator access required");
        return;
    }

    const bool needsAdmin = (action == "add_operator" || action == "remove_operator" ||
                             action == "change_password" || action == "factory_reset" ||
                             action == "get_operators" || action == "set_timezone" ||
                             action == "save_settings" ||
                             action == "save_helloclub_settings" ||
                             action == "save_qr_settings" ||
                             action == "get_helloclub_settings");
    if (needsAdmin && clientRole < ADMIN) {
        sendError(client, "Admin access required");
        return;
    }

    // --- Timer actions ---

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
            startDoc["numRounds"] = timer.getNumRounds();
            startDoc["currentRound"] = timer.getCurrentRound();
            startDoc["continuousMode"] = timer.getContinuousMode();
            startDoc["pauseAfterNext"] = timer.getPauseAfterNext();
            String output;
            serializeJson(startDoc, output);
            ws.textAll(output);
        }
    } else if (action == "pause") {
        if (timer.getState() == RUNNING) {
            timer.pause();
            StaticJsonDocument<256> pauseDoc;
            pauseDoc["event"] = "pause";
            pauseDoc["mainTimerRemaining"] = timer.getMainTimerRemaining();
            String pauseOut;
            serializeJson(pauseDoc, pauseOut);
            ws.textAll(pauseOut);
        } else if (timer.getState() == PAUSED) {
            timer.resume();
            StaticJsonDocument<256> resumeDoc;
            resumeDoc["event"] = "resume";
            resumeDoc["mainTimerRemaining"] = timer.getMainTimerRemaining();
            String resumeOut;
            serializeJson(resumeDoc, resumeOut);
            ws.textAll(resumeOut);
        }
    } else if (action == "reset") {
        // If resetting during an active HC event, persist cancel flag
        // so boot recovery won't re-trigger this event
        if (!activeEventId.isEmpty()) {
            Preferences cancelPrefs;
            if (cancelPrefs.begin("helloclub", false)) {
                cancelPrefs.putString("evt_cancel", activeEventId);
                cancelPrefs.end();
            }
            DEBUG_PRINTF("Reset during event %s — cancel flag saved\n", activeEventId.c_str());
        }
        timer.reset();
        activeEventEndTime = 0;
        activeEventName = "";
        activeEventId = "";
        sendEvent("reset");

    } else if (action == "pause_after_next") {
        bool enabled = doc["enabled"] | false;
        timer.setPauseAfterNext(enabled);

        StaticJsonDocument<256> panDoc;
        panDoc["event"] = "pause_after_next_changed";
        panDoc["enabled"] = enabled;
        String output;
        serializeJson(panDoc, output);
        ws.textAll(output);

    } else if (action == "save_settings") {
        JsonObject settingsObj = doc["settings"];

        unsigned long gameDurMs = settingsObj["gameDuration"].as<unsigned long>();  // Client sends milliseconds
        unsigned int rounds = settingsObj["numRounds"].as<unsigned int>();
        unsigned long sirenLen = settingsObj["sirenLength"].as<unsigned long>();
        unsigned long sirenPau = settingsObj["sirenPause"].as<unsigned long>();

        unsigned long gameDurMin = gameDurMs / 60000;  // Convert to minutes for validation
        if (gameDurMin < 1 || gameDurMin > 120) {
            sendError(client, "Game duration must be between 1 and 120 minutes");
            return;
        }
        if (rounds < 1 || rounds > 20) {
            sendError(client, "Number of rounds must be between 1 and 20");
            return;
        }
        if (sirenLen < 100 || sirenLen > 10000) {
            sendError(client, "Siren length must be between 100 and 10000 ms");
            return;
        }
        if (sirenPau < 100 || sirenPau > 10000) {
            sendError(client, "Siren pause must be between 100 and 10000 ms");
            return;
        }

        timer.setGameDuration(gameDurMs);
        timer.setNumRounds(rounds);
        siren.setBlastLength(sirenLen);
        siren.setBlastPause(sirenPau);

        settings.save(timer, siren);
        sendSettingsUpdate();

    } else if (action == "set_timezone") {
        String timezone = doc["timezone"] | "";

        if (timezone.length() == 0) {
            sendError(client, "Timezone cannot be empty");
            return;
        }

        if (settings.setTimezone(timezone)) {
            myTZ.setLocation(timezone);
            Serial.printf("Timezone changed to: %s\n", timezone.c_str());

            StaticJsonDocument<256> successDoc;
            successDoc["event"] = "timezone_changed";
            successDoc["timezone"] = timezone;
            successDoc["message"] = "Timezone updated successfully";
            String output;
            serializeJson(successDoc, output);
            client->text(output);

            sendNTPStatus();
        } else {
            sendError(client, "Failed to set timezone");
        }

    // --- User management ---

    } else if (action == "add_operator") {
        String username = doc["username"] | "";
        String password = doc["password"] | "";

        if (username.length() == 0) {
            sendError(client, "Username cannot be empty");
        } else if (password.length() == 0) {
            sendError(client, "Password cannot be empty");
        } else if (password.length() < MIN_PASSWORD_LENGTH) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Password must be at least %d characters", MIN_PASSWORD_LENGTH);
            sendError(client, msg);
        } else if (userManager.usernameExists(username)) {
            sendError(client, "Username already exists");
        } else if (userManager.addOperator(username, password)) {
            StaticJsonDocument<256> successDoc;
            successDoc["event"] = "operator_added";
            successDoc["username"] = username;
            String output;
            serializeJson(successDoc, output);
            client->text(output);
        } else {
            sendError(client, "Maximum number of operators reached");
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
        userManager.factoryReset();
        timer.setGameDuration(DEFAULT_GAME_DURATION);
        timer.setNumRounds(DEFAULT_NUM_ROUNDS);
        siren.setBlastLength(DEFAULT_SIREN_LENGTH);
        siren.setBlastPause(DEFAULT_SIREN_PAUSE);
        settings.save(timer, siren);
        timer.reset();
        activeEventEndTime = 0;
        activeEventName = "";
        activeEventId = "";
        // Clear cancel flag
        {
            Preferences cancelPrefs;
            if (cancelPrefs.begin("helloclub", false)) {
                cancelPrefs.remove("evt_cancel");
                cancelPrefs.end();
            }
        }

        StaticJsonDocument<256> resetDoc;
        resetDoc["event"] = "factory_reset_complete";
        resetDoc["message"] = "System reset to factory defaults";
        String output;
        serializeJson(resetDoc, output);
        ws.textAll(output);

        for (auto it = authenticatedClients.begin(); it != authenticatedClients.end();) {
            if (it->second != VIEWER) {
                it = authenticatedClients.erase(it);
            } else {
                ++it;
            }
        }

        sendSettingsUpdate();

    // --- Hello Club ---

    } else if (action == "get_upcoming_events") {
        sendUpcomingEvents(client);

    } else if (action == "get_helloclub_settings") {
        // Permission already checked in needsAdmin block above
        StaticJsonDocument<512> settingsDoc;
        settingsDoc["event"] = "helloclub_settings";
        settingsDoc["apiKey"] = helloClubApiKey.isEmpty() ? "" : "***configured***";
        settingsDoc["enabled"] = helloClubEnabled;
        settingsDoc["defaultDuration"] = settings.getHcDefaultDuration();
        String output;
        serializeJson(settingsDoc, output);
        client->text(output);

    } else if (action == "save_helloclub_settings") {
        if (doc.containsKey("apiKey")) {
            String newApiKey = doc["apiKey"].as<String>();
            if (!newApiKey.isEmpty() && newApiKey != "***configured***") {
                helloClubApiKey = newApiKey;
            }
        }
        if (doc.containsKey("enabled")) {
            helloClubEnabled = doc["enabled"].as<bool>();
        }
        if (doc.containsKey("defaultDuration")) {
            uint16_t dur = doc["defaultDuration"].as<uint16_t>();
            if (dur >= 1 && dur <= 120) {
                settings.setHcDefaultDuration(dur);
                helloClubClient.setDefaults(dur, DEFAULT_NUM_ROUNDS);
            }
        }

        saveHelloClubSettings();

        StaticJsonDocument<256> successDoc;
        successDoc["event"] = "helloclub_settings_saved";
        successDoc["message"] = "Hello Club settings saved successfully";
        String output;
        serializeJson(successDoc, output);
        client->text(output);

    } else if (action == "helloclub_refresh") {
        if (helloClubClient.fetchAndCacheEvents(HELLOCLUB_DAYS_AHEAD, myTZ)) {
            lastHelloClubPoll = millis();
            lastHelloClubPollFailed = false;

            DynamicJsonDocument successDoc(2048);
            successDoc["event"] = "helloclub_refresh_result";
            successDoc["success"] = true;
            successDoc["message"] = String("Synced: ") + helloClubClient.getCachedEvents().size() +
                " with timer: tag out of " + helloClubClient.getTotalEventsFromApi() + " total events";
            successDoc["eventCount"] = helloClubClient.getCachedEvents().size();
            successDoc["totalEvents"] = helloClubClient.getTotalEventsFromApi();
            successDoc["debug"] = helloClubClient.getLastSyncDebug();
            String output;
            serializeJson(successDoc, output);
            client->text(output);

            sendUpcomingEvents(); // Broadcast to all
        } else {
            DynamicJsonDocument failDoc(2048);
            failDoc["event"] = "helloclub_refresh_result";
            failDoc["success"] = false;
            failDoc["message"] = helloClubClient.getLastError();
            failDoc["debug"] = helloClubClient.getLastSyncDebug();
            String output;
            serializeJson(failDoc, output);
            client->text(output);
        }

    // --- QR Config ---

    } else if (action == "get_qr_config") {
        StaticJsonDocument<512> qrDoc;
        qrDoc["event"] = "qr_config";
        // Use override SSID if set, otherwise fall back to connected network SSID
        String ssidOverride = settings.getGuestWifiSsid();
        qrDoc["ssid"] = ssidOverride.isEmpty() ? WiFi.SSID() : ssidOverride;
        qrDoc["ssidOverride"] = ssidOverride;  // Send override separately so UI can show it in the field
        qrDoc["connectedSsid"] = WiFi.SSID();  // Always send actual connected SSID as hint
        qrDoc["password"] = settings.getGuestWifiPass();
        qrDoc["encryption"] = settings.getGuestWifiEnc();
        qrDoc["appUrl"] = "http://" + WiFi.localIP().toString() + "/";
        String output;
        serializeJson(qrDoc, output);
        client->text(output);

    } else if (action == "save_qr_settings") {
        String pass = doc["password"] | "";
        String enc = doc["encryption"] | "WPA";
        String ssid = doc["ssid"] | "";

        if (enc != "WPA" && enc != "WEP" && enc != "nopass") {
            sendError(client, "Invalid encryption type");
            return;
        }

        if (settings.saveQrSettings(pass, enc, ssid)) {
            StaticJsonDocument<256> successDoc;
            successDoc["event"] = "qr_settings_saved";
            successDoc["message"] = "QR settings saved";
            String output;
            serializeJson(successDoc, output);
            client->text(output);
        } else {
            sendError(client, "Failed to save QR settings");
        }
    }
}


void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch(type) {
        case WS_EVT_CONNECT: {
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            authenticatedClients[client->id()] = VIEWER;
            clientRateLimits[client->id()] = {millis(), 0};
            clientLastActivity[client->id()] = millis();
            StaticJsonDocument<256> loginDoc;
            loginDoc["event"] = "login_prompt";
            loginDoc["message"] = "Welcome! Login for full access or continue as viewer.";
            String output;
            serializeJson(loginDoc, output);
            client->text(output);
            sendSettingsUpdate(client);
            if (timer.getState() == RUNNING || timer.getState() == PAUSED) {
                sendSync(client);
            } else {
                sendStateUpdate(client);
            }
            sendNTPStatus(client);
            break;
        }

        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            authenticatedClients.erase(client->id());
            authenticatedUsernames.erase(client->id());
            clientRateLimits.erase(client->id());
            clientLastActivity.erase(client->id());
            break;

        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len, client);
            break;

        case WS_EVT_ERROR:
            Serial.printf("WebSocket client #%u error\n", client->id());
            break;

        case WS_EVT_PONG:
            break;
    }
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
      else
        type = "filesystem";
      DEBUG_PRINTLN("Start updating " + type);

      // Stop siren and pause timer before flashing to prevent siren stuck on
      siren.stop();
      if (timer.getState() == RUNNING) {
          timer.pause();
      }

      if (ENABLE_WATCHDOG) {
          esp_task_wdt_reset();
      }
    })
    .onEnd([]() {
      DEBUG_PRINTLN("\nOTA Update complete");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      DEBUG_PRINTF("Progress: %u%%\r", (progress / (total / 100)));
      if (ENABLE_WATCHDOG) {
          esp_task_wdt_reset();
      }
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

void setupWatchdog() {
    esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);
    DEBUG_PRINTF("Watchdog timer enabled (%lu second timeout)\n", WATCHDOG_TIMEOUT_SEC);
}

// ==========================================================================
// --- Self-Test ---
// ==========================================================================

void runSelfTest() {
    DEBUG_PRINTLN("Running self-test...");
    bool allTestsPassed = true;

    DEBUG_PRINT("  Testing SPIFFS... ");
    if (SPIFFS.begin(true)) {
        DEBUG_PRINTLN("PASS");
    } else {
        DEBUG_PRINTLN("FAIL");
        allTestsPassed = false;
    }

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

    DEBUG_PRINT("  Testing Relay pin... ");
    if (digitalRead(RELAY_PIN) == LOW) {
        DEBUG_PRINTLN("PASS (pin LOW, relay off)");
    } else {
        DEBUG_PRINTLN("WARNING (pin HIGH - forcing LOW)");
        digitalWrite(RELAY_PIN, LOW);
    }

    if (allTestsPassed) {
        DEBUG_PRINTLN("Self-test PASSED\n");
    } else {
        DEBUG_PRINTLN("Self-test FAILED - Some components may not function correctly\n");
    }
}

// ==========================================================================
// --- Hello Club Integration ---
// ==========================================================================

void loadHelloClubSettings() {
    Preferences prefs;
    if (prefs.begin("helloclub", true)) {
        helloClubApiKey = prefs.getString("apiKey", "");
        helloClubEnabled = prefs.getBool("enabled", false);
        prefs.end();

        DEBUG_PRINTLN("Hello Club settings loaded:");
        DEBUG_PRINTF("  Enabled: %s\n", helloClubEnabled ? "Yes" : "No");

        helloClubClient.setApiKey(helloClubApiKey);
    }
}

void saveHelloClubSettings() {
    Preferences prefs;
    if (prefs.begin("helloclub", false)) {
        prefs.putString("apiKey", helloClubApiKey);
        prefs.putBool("enabled", helloClubEnabled);
        prefs.end();

        DEBUG_PRINTLN("Hello Club settings saved");
        helloClubClient.setApiKey(helloClubApiKey);
    }
}

// FreeRTOS task: runs Hello Club HTTP fetch on a background core
void hcFetchTask(void* param) {
    bool success = helloClubClient.fetchAndCacheEvents(HELLOCLUB_DAYS_AHEAD, myTZ);
    hcFetchResultSuccess = success;
    hcFetchResultReady = true;
    hcFetchInProgress = false;
    hcFetchTaskHandle = nullptr;
    vTaskDelete(nullptr); // Self-delete
}

void checkHelloClubPoll() {
    if (!helloClubEnabled || !helloClubClient.isConfigured()) {
        return;
    }

    if (!timeStatus()) {
        return; // Wait for NTP sync
    }

    // Check if background fetch completed
    if (hcFetchResultReady) {
        hcFetchResultReady = false;
        unsigned long now = millis();
        lastHelloClubPoll = now;

        if (hcFetchResultSuccess) {
            lastHelloClubPollFailed = false;
            DEBUG_PRINTLN("HelloClub: Background poll successful");
            sendUpcomingEvents();
        } else {
            lastHelloClubPollFailed = true;
            DEBUG_PRINTF("HelloClub: Background poll failed - %s (retry in 5min)\n",
                         helloClubClient.getLastError().c_str());
        }
        return;
    }

    // Don't start a new fetch if one is already running
    if (hcFetchInProgress) {
        return;
    }

    unsigned long now = millis();
    unsigned long interval = lastHelloClubPollFailed ? HELLOCLUB_RETRY_INTERVAL_MS : HELLOCLUB_POLL_INTERVAL_MS;

    if (lastHelloClubPoll > 0 && now - lastHelloClubPoll < interval) {
        return; // Not time yet
    }

    DEBUG_PRINTLN("HelloClub: Starting background poll...");
    hcFetchInProgress = true;
    hcFetchResultReady = false;

    // Launch on core 0 (network core) — main loop runs on core 1
    xTaskCreatePinnedToCore(
        hcFetchTask,        // Task function
        "hcFetch",          // Name
        8192,               // Stack size (bytes) — SSL needs ~6KB
        nullptr,            // Parameter
        1,                  // Priority (low — don't starve WiFi)
        &hcFetchTaskHandle, // Handle
        0                   // Core 0 (protocol core)
    );
}
