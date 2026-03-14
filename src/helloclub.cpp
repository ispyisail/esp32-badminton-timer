#include "helloclub.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <time.h>

// Root CA Certificates — Google Trust Services Root R4 + Let's Encrypt ISRG Root X1
// Multiple roots for resilience against CA changes
const char* rootCACertificate = \
// Google Trust Services Root R4 (ECDSA) — used by api.helloclub.com — valid until 2036-06-22
"-----BEGIN CERTIFICATE-----\n" \
"MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD\n" \
"VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG\n" \
"A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw\n" \
"WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz\n" \
"IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi\n" \
"AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi\n" \
"QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR\n" \
"HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW\n" \
"BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D\n" \
"9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8\n" \
"p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD\n" \
"-----END CERTIFICATE-----\n" \
// Let's Encrypt ISRG Root X1 — valid until 2035-06-04
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";

const char* HelloClubClient::NVS_NAMESPACE = "helloclub";
const char* HelloClubClient::NVS_EVENTS_KEY = "events";

HelloClubClient::HelloClubClient()
    : apiKey("")
    , lastError("")
    , lastSyncTime(0)
    , defaultDurationMin(12)
    , defaultNumRounds(3)
{
}

void HelloClubClient::setApiKey(const String& key) {
    apiKey = key;
}

void HelloClubClient::setDefaults(uint16_t defaultDuration, uint8_t defaultRounds) {
    defaultDurationMin = defaultDuration;
    defaultNumRounds = defaultRounds;
}

bool HelloClubClient::makeRequest(const String& endpoint, const String& params, DynamicJsonDocument& responseDoc) {
    if (apiKey.isEmpty()) {
        lastError = "API key not configured";
        return false;
    }

    const int MAX_RETRIES = 3;
    const int RETRY_DELAYS[] = {1000, 2000, 4000};

    String url = baseUrl + endpoint;
    if (!params.isEmpty()) {
        url += "?" + params;
    }

    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        DEBUG_PRINTF("HelloClub API Request (attempt %d/%d): %s\n", attempt + 1, MAX_RETRIES, url.c_str());

        WiFiClientSecure client;
        client.setCACert(rootCACertificate);

        HTTPClient http;

        if (!http.begin(client, url)) {
            lastError = "Failed to begin HTTP request";
            if (attempt < MAX_RETRIES - 1) {
                delay(RETRY_DELAYS[attempt]);
                continue;
            }
            return false;
        }

        http.addHeader("X-Api-Key", apiKey);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Accept", "application/json");
        http.setTimeout(10000);

        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            http.end();

            DeserializationError error = deserializeJson(responseDoc, payload);
            if (error) {
                lastError = "JSON parse error: " + String(error.c_str());
                return false;
            }

            DEBUG_PRINTF("HelloClub API: Success on attempt %d\n", attempt + 1);
            return true;
        }

        http.end();

        bool shouldRetry = (httpCode == 429 || httpCode == 503 || httpCode == 504 || httpCode < 0);

        lastError = "HTTP error: " + String(httpCode);
        if (httpCode == 401) {
            lastError += " (Invalid API key)";
            shouldRetry = false;
        } else if (httpCode == 429) {
            lastError += " (Rate limit exceeded)";
        }

        if (!shouldRetry || attempt == MAX_RETRIES - 1) {
            DEBUG_PRINTF("HelloClub API: Failed - %s\n", lastError.c_str());
            return false;
        }

        DEBUG_PRINTF("HelloClub API: Retrying in %dms...\n", RETRY_DELAYS[attempt]);
        delay(RETRY_DELAYS[attempt]);
    }

    return false;
}

bool HelloClubClient::parseTimerTag(const String& description, uint16_t& duration, uint8_t& rounds) {
    // Look for "timer:" in description (case-insensitive)
    String lower = description;
    lower.toLowerCase();

    int tagPos = lower.indexOf("timer:");
    if (tagPos == -1) {
        return false; // No timer tag = don't cache this event
    }

    // Extract the value after "timer:"
    String value = description.substring(tagPos + 6);
    value.trim();

    // Truncate at newline or end
    int nlPos = value.indexOf('\n');
    if (nlPos > 0) value = value.substring(0, nlPos);
    value.trim();

    // "timer:enabled" or "timer:" -> use defaults, continuous mode
    if (value.startsWith("enabled") || value.isEmpty()) {
        duration = defaultDurationMin;
        rounds = 0; // 0 = continuous (rounds repeat until event ends)
        return true;
    }

    // "timer: 15min" -> custom duration, continuous unless rounds specified
    duration = defaultDurationMin;
    rounds = 0; // 0 = continuous by default

    // Parse Xmin
    int minPos = lower.indexOf("min", tagPos + 6);
    if (minPos > tagPos + 6) {
        String numStr = "";
        for (int i = minPos - 1; i >= tagPos + 6; i--) {
            char c = value.charAt(i - (tagPos + 6));
            if (c >= '0' && c <= '9') {
                numStr = String(c) + numStr;
            } else {
                break;
            }
        }
        if (numStr.length() > 0) {
            int val = numStr.toInt();
            if (val >= 1 && val <= 120) {
                duration = val;
            }
        }
    }

    // Parse Xrounds (optional — if omitted, continuous mode)
    int roundPos = lower.indexOf("round", tagPos + 6);
    if (roundPos > tagPos + 6) {
        String numStr = "";
        for (int i = roundPos - 1; i >= tagPos + 6; i--) {
            char c = value.charAt(i - (tagPos + 6));
            if (c >= '0' && c <= '9') {
                numStr = String(c) + numStr;
            } else {
                break;
            }
        }
        if (numStr.length() > 0) {
            int val = numStr.toInt();
            if (val >= 1 && val <= 20) {
                rounds = val; // Non-zero = fixed round count
            }
        }
    }

    return true;
}

time_t HelloClubClient::parseISOToEpoch(const String& isoDate) {
    if (isoDate.length() < 19) {
        return 0;
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = isoDate.substring(0, 4).toInt() - 1900;
    tm.tm_mon = isoDate.substring(5, 7).toInt() - 1;
    tm.tm_mday = isoDate.substring(8, 10).toInt();
    tm.tm_hour = isoDate.substring(11, 13).toInt();
    tm.tm_min = isoDate.substring(14, 16).toInt();
    tm.tm_sec = isoDate.substring(17, 19).toInt();

    // mktime interprets as local time, but we want UTC
    // Use timegm equivalent
    time_t result = mktime(&tm);
    // Adjust for local timezone offset to get UTC
    struct tm utc_check;
    gmtime_r(&result, &utc_check);
    time_t utc_result = mktime(&utc_check);
    time_t offset = utc_result - result;
    result -= offset;

    return result;
}

bool HelloClubClient::fetchAndCacheEvents(int daysAhead, Timezone& tz) {
    lastError = "";

    // Calculate date range
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    char fromDate[30];
    strftime(fromDate, sizeof(fromDate), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    time_t future = now + (daysAhead * 24 * 60 * 60);
    gmtime_r(&future, &timeinfo);

    char toDate[30];
    strftime(toDate, sizeof(toDate), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    String params = "fromDate=" + String(fromDate);
    params += "&toDate=" + String(toDate);
    params += "&sort=startDate";
    params += "&limit=50";

    DynamicJsonDocument responseDoc(16384);
    if (!makeRequest("/event", params, responseDoc)) {
        return false;
    }

    JsonArray eventsArray = responseDoc["events"].as<JsonArray>();
    if (eventsArray.isNull()) {
        lastError = "No events array in response";
        return false;
    }

    DEBUG_PRINTF("HelloClub: Found %d events from API\n", eventsArray.size());

    // Build new event list, keeping triggered state from existing cache
    std::vector<CachedEvent> newEvents;

    for (JsonObject eventObj : eventsArray) {
        // Get description to check for timer: tag
        String description = "";
        if (eventObj.containsKey("description")) {
            description = eventObj["description"].as<String>();
        }

        uint16_t duration;
        uint8_t rounds;
        if (!parseTimerTag(description, duration, rounds)) {
            continue; // Skip events without timer: tag
        }

        if (newEvents.size() >= HC_MAX_EVENTS) {
            break;
        }

        CachedEvent evt;
        String fullId = eventObj["id"].as<String>();
        evt.id = fullId.substring(0, 12);
        evt.name = eventObj["name"].as<String>();
        if (evt.name.length() > 40) {
            evt.name = evt.name.substring(0, 40);
        }

        evt.startTime = parseISOToEpoch(eventObj["startDate"].as<String>());
        evt.endTime = parseISOToEpoch(eventObj["endDate"].as<String>());
        evt.durationMin = duration;
        evt.numRounds = rounds;

        // Preserve triggered state from existing cache
        evt.triggered = false;
        for (const auto& existing : events) {
            if (existing.id == evt.id) {
                evt.triggered = existing.triggered;
                break;
            }
        }

        newEvents.push_back(evt);
        DEBUG_PRINTF("  Cached: %s (%s) %dmin %drounds\n",
                     evt.name.c_str(), evt.id.c_str(), evt.durationMin, evt.numRounds);
    }

    events = newEvents;
    lastSyncTime = millis();
    saveToNVS();

    DEBUG_PRINTF("HelloClub: Cached %d events with timer: tag\n", events.size());
    return true;
}

void HelloClubClient::loadFromNVS() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        DEBUG_PRINTLN("HelloClub: No cached events in NVS");
        return;
    }

    String json = prefs.getString(NVS_EVENTS_KEY, "[]");
    prefs.end();

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        DEBUG_PRINTF("HelloClub: Failed to parse NVS cache: %s\n", error.c_str());
        return;
    }

    events.clear();
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        CachedEvent evt;
        evt.id = obj["i"].as<String>();
        evt.name = obj["n"].as<String>();
        evt.startTime = obj["s"].as<time_t>();
        evt.endTime = obj["e"].as<time_t>();
        evt.durationMin = obj["d"].as<uint16_t>();
        evt.numRounds = obj["r"].as<uint8_t>();
        evt.triggered = obj["t"].as<bool>();
        events.push_back(evt);
    }

    DEBUG_PRINTF("HelloClub: Loaded %d cached events from NVS\n", events.size());
}

void HelloClubClient::saveToNVS() {
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();

    for (const auto& evt : events) {
        JsonObject obj = arr.createNestedObject();
        obj["i"] = evt.id;
        obj["n"] = evt.name;
        obj["s"] = (long)evt.startTime;
        obj["e"] = (long)evt.endTime;
        obj["d"] = evt.durationMin;
        obj["r"] = evt.numRounds;
        obj["t"] = evt.triggered;
    }

    String json;
    serializeJson(doc, json);

    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.putString(NVS_EVENTS_KEY, json);
        prefs.end();
        DEBUG_PRINTF("HelloClub: Saved %d events to NVS (%d bytes)\n", events.size(), json.length());
    }
}

CachedEvent* HelloClubClient::checkAutoTrigger(Timezone& tz) {
    time_t now = time(nullptr);

    for (auto& evt : events) {
        if (evt.triggered) continue;

        // Check if now is within the trigger window (startTime to startTime + 2 min)
        if (now >= evt.startTime && now <= evt.startTime + (HELLOCLUB_TRIGGER_WINDOW_MS / 1000)) {
            return &evt;
        }
    }

    return nullptr;
}

void HelloClubClient::markTriggered(const String& id) {
    for (auto& evt : events) {
        if (evt.id == id) {
            evt.triggered = true;
            DEBUG_PRINTF("HelloClub: Marked event '%s' as triggered\n", evt.name.c_str());
            saveToNVS();
            return;
        }
    }
}

void HelloClubClient::purgeExpired(Timezone& tz) {
    time_t now = time(nullptr);
    size_t before = events.size();

    events.erase(
        std::remove_if(events.begin(), events.end(),
            [now](const CachedEvent& evt) { return evt.endTime < now; }),
        events.end()
    );

    if (events.size() < before) {
        DEBUG_PRINTF("HelloClub: Purged %d expired events\n", before - events.size());
        saveToNVS();
    }
}
