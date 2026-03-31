#include "helloclub.h"
#include "config.h"
#include "remotelog.h"
#include <WiFiClientSecure.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

bool HelloClubClient::makeRequest(const String& endpoint, const String& params,
                                   DynamicJsonDocument& responseDoc, const JsonDocument& filter) {
    if (apiKey.isEmpty()) {
        lastError = "API key not configured";
        return false;
    }

    const int MAX_RETRIES = 2;          // Reduced to minimize blocking time
    const int RETRY_DELAYS[] = {500, 1000};  // Short delays to avoid freezing main loop

    String url = baseUrl + endpoint;
    if (!params.isEmpty()) {
        url += "?" + params;
    }

    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        DEBUG_PRINTF("HelloClub API (attempt %d/%d): %s\n", attempt + 1, MAX_RETRIES, url.c_str());

        WiFiClientSecure client;
        client.setCACert(rootCACertificate);

        HTTPClient http;

        if (!http.begin(client, url)) {
            lastError = "Failed to begin HTTP request";
            if (attempt < MAX_RETRIES - 1) {
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAYS[attempt]));
                continue;
            }
            return false;
        }

        http.addHeader("X-Api-Key", apiKey);
        http.addHeader("Accept", "application/json");
        http.setTimeout(5000);

        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            int contentLen = http.getSize();
            DEBUG_PRINTF("HelloClub API: %d bytes, heap: %d\n", contentLen, ESP.getFreeHeap());

            // Reject oversized responses to prevent OOM
            // contentLen == -1 means chunked/unknown, allow up to heap check below
            if (contentLen > 32768) {
                lastError = "Response too large: " + String(contentLen) + " bytes";
                http.end();
                return false;
            }

            // Check we have enough heap to hold the payload + parsing overhead
            uint32_t freeHeap = ESP.getFreeHeap();
            int estimatedNeed = (contentLen > 0 ? contentLen : 8192) * 2; // payload + JSON parse
            if (freeHeap < (uint32_t)estimatedNeed + 10240) {
                lastError = "Insufficient heap: " + String(freeHeap) + " free, need ~" + String(estimatedNeed);
                http.end();
                return false;
            }

            // Read payload, then free SSL connection before parsing
            String payload = http.getString();
            http.end();
            client.stop();

            // Parse with filter — only keeps specified fields, dramatically reduces memory
            DeserializationError error = deserializeJson(responseDoc, payload,
                DeserializationOption::Filter(filter));
            if (error) {
                lastError = "JSON parse error: " + String(error.c_str()) +
                    " (" + payload.length() + "B payload, " + ESP.getFreeHeap() + "B heap)";
                return false;
            }

            payload = String();  // Free payload
            DEBUG_PRINTF("HelloClub API: OK, heap: %d\n", ESP.getFreeHeap());
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
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAYS[attempt]));
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

    // Work with lowercased value (not the full description 'lower')
    // to keep indices consistent after trim/truncation
    String lowerValue = value;
    lowerValue.toLowerCase();

    // Parse Xmin
    int minPos = lowerValue.indexOf("min");
    if (minPos > 0) {
        String numStr = "";
        for (int i = minPos - 1; i >= 0; i--) {
            char c = lowerValue.charAt(i);
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
    int roundPos = lowerValue.indexOf("round");
    if (roundPos > 0) {
        String numStr = "";
        for (int i = roundPos - 1; i >= 0; i--) {
            char c = lowerValue.charAt(i);
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

    int year = isoDate.substring(0, 4).toInt();
    int mon  = isoDate.substring(5, 7).toInt();
    int day  = isoDate.substring(8, 10).toInt();
    int hour = isoDate.substring(11, 13).toInt();
    int min  = isoDate.substring(14, 16).toInt();
    int sec  = isoDate.substring(17, 19).toInt();

    // Convert to UTC epoch without relying on mktime (which uses local TZ
    // and gets DST wrong on ESP32). Algorithm: days since 1970-01-01.
    // Months Jan=1..Dec=12; shift Mar=1 so leap day falls at year end.
    int y = year;
    int m = mon;
    if (m <= 2) { y--; m += 12; }
    m -= 3; // Mar=0 .. Feb=11

    // Days from epoch to date (calibrated: 1970-01-01 = 0)
    long days = 365L * (y - 1970)
              + (y - 1968) / 4 - (y - 1900) / 100 + (y - 1600) / 400
              + (153 * m + 2) / 5 + day - 1
              + 59;

    return (time_t)(days * 86400L + hour * 3600L + min * 60L + sec);
}

bool HelloClubClient::fetchAndCacheEvents(int daysAhead, Timezone& tz) {
    lastError = "";

    // Calculate date range using ezTime (C time(nullptr) may not be NTP-synced)
    time_t now = UTC.now();
    if (now < 1000000000) { // Before ~2001 = NTP not synced
        lastError = "NTP time not synced yet";
        remoteLog("HC fetch: NTP not synced (now=%ld)", (long)now);
        return false;
    }
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    char fromDate[30];
    strftime(fromDate, sizeof(fromDate), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    time_t future = now + (daysAhead * 24 * 60 * 60);
    gmtime_r(&future, &timeinfo);

    char toDate[30];
    strftime(toDate, sizeof(toDate), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    lastSyncDebug = "";
    lastSyncDebug.reserve(512);
    char syncBuf[128];
    snprintf(syncBuf, sizeof(syncBuf), "Query: %s to %s\n", fromDate, toDate);
    lastSyncDebug += syncBuf;

    // JSON filter — only keep the fields we need (saves ~90% memory)
    StaticJsonDocument<256> filter;
    filter["events"][0]["id"] = true;
    filter["events"][0]["name"] = true;
    filter["events"][0]["description"] = true;
    filter["events"][0]["startDate"] = true;
    filter["events"][0]["endDate"] = true;

    // Paginate: fetch PAGE_SIZE events at a time to stay within ESP32 RAM
    const int PAGE_SIZE = 5;
    totalEventsFromApi = 0;
    std::vector<CachedEvent> newEvents;

    for (int offset = 0; offset < 100; offset += PAGE_SIZE) {  // Safety cap at 100
        String params = "fromDate=" + String(fromDate);
        params += "&toDate=" + String(toDate);
        params += "&sort=startDate";
        params += "&limit=" + String(PAGE_SIZE);
        params += "&offset=" + String(offset);

        // Small doc — filter strips everything except our 5 fields
        DynamicJsonDocument responseDoc(8192);
        if (!makeRequest("/event", params, responseDoc, filter)) {
            if (offset == 0) {
                // First page failed — report error
                lastSyncDebug += "Page 1 failed: " + lastError + "\n";
                return false;
            }
            // Later pages failing is OK — we got some events
            DEBUG_PRINTF("HelloClub: Page at offset %d failed, stopping pagination\n", offset);
            break;
        }

        // Try both response formats: {"events": [...]} or top-level array
        JsonArray eventsArray;
        if (responseDoc.containsKey("events")) {
            eventsArray = responseDoc["events"].as<JsonArray>();
        } else if (responseDoc.is<JsonArray>()) {
            eventsArray = responseDoc.as<JsonArray>();
        }

        if (eventsArray.isNull() || eventsArray.size() == 0) {
            DEBUG_PRINTF("HelloClub: No more events at offset %d\n", offset);
            break;  // No more events
        }

        int pageCount = eventsArray.size();
        totalEventsFromApi += pageCount;
        DEBUG_PRINTF("HelloClub: Page offset=%d returned %d events\n", offset, pageCount);

        for (JsonObject eventObj : eventsArray) {
            String description = eventObj["description"] | "";
            String name = eventObj["name"] | "unnamed";

            // Capture debug info using fixed buffer to avoid String fragmentation
            if (lastSyncDebug.length() < 400) {
                char debugLine[96];
                snprintf(debugLine, sizeof(debugLine), "%.30s | %.50s\n",
                    name.c_str(),
                    description.length() > 0 ? description.c_str() : "(no desc)");
                lastSyncDebug += debugLine;
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
            String fullId = eventObj["id"] | "";
            evt.id = fullId.substring(0, 12);
            evt.name = name.substring(0, 40);
            evt.startTime = parseISOToEpoch(eventObj["startDate"] | "");
            evt.endTime = parseISOToEpoch(eventObj["endDate"] | "");
            evt.durationMin = duration;
            evt.numRounds = rounds;

            evt.triggered = false;
            newEvents.push_back(evt);
            DEBUG_PRINTF("  Cached: %s %dmin %drounds\n",
                         evt.name.c_str(), evt.durationMin, evt.numRounds);
        }

        // If we got fewer than PAGE_SIZE, that's the last page
        if (pageCount < PAGE_SIZE) {
            break;
        }

        // Don't exceed max cached events
        if (newEvents.size() >= HC_MAX_EVENTS) {
            break;
        }
    }

    char summaryBuf[80];
    snprintf(summaryBuf, sizeof(summaryBuf), "Total: %d events, %d with timer: tag\n",
        totalEventsFromApi, (int)newEvents.size());
    lastSyncDebug += summaryBuf;

    // Stage the results — don't modify `events` directly as this may run
    // on a background task while the main loop reads `events`
    stagedEvents = newEvents;
    stagedReady = true;

    DEBUG_PRINTF("HelloClub: Fetched %d events with timer: tag (from %d total), staged for apply\n",
                 (int)newEvents.size(), totalEventsFromApi);
    return true;
}

bool HelloClubClient::applyStagedEvents() {
    if (!stagedReady) return false;
    stagedReady = false;

    // Preserve triggered state from existing cache (safe: runs on main loop)
    // Match on both id AND startTime so recurring events don't cross-contaminate
    for (auto& staged : stagedEvents) {
        for (const auto& existing : events) {
            if (existing.id == staged.id && existing.startTime == staged.startTime) {
                staged.triggered = existing.triggered;
                break;
            }
        }
    }

    events = stagedEvents;
    stagedEvents.clear();
    lastSyncTime = millis();
    saveToNVS();

    remoteLog("HC fetch: %d total, %d with timer tag", totalEventsFromApi, (int)events.size());
    for (const auto& e : events) {
        remoteLog("HC evt: \"%s\" start=%ld trig=%d", e.name.c_str(), (long)e.startTime, e.triggered ? 1 : 0);
    }
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

RecoveryResult HelloClubClient::checkMidEventRecovery() {
    RecoveryResult result;
    result.shouldRecover = false;

    time_t now = UTC.now();
    if (now < 1000000000) {
        return result; // NTP not synced
    }

    CachedEvent* bestEvent = nullptr;

    remoteLog("Recovery scan: %d events, now=%ld", (int)events.size(), (long)now);
    for (auto& evt : events) {
        // Only recover events that were actually auto-started before the reboot
        if (!evt.triggered) {
            remoteLog("Recovery skip \"%s\": never triggered", evt.name.c_str());
            continue;
        }

        // Must have started
        if (now < evt.startTime) {
            remoteLog("Recovery skip \"%s\": not started yet (start=%ld)", evt.name.c_str(), (long)evt.startTime);
            continue;
        }

        time_t elapsed = now - evt.startTime;
        unsigned long roundDurationSec = (unsigned long)evt.durationMin * 60;

        // Calculate actual play window from timer: tag (not booking endTime)
        // For short bookings (endTime ≈ startTime), the timer duration is what matters
        unsigned long totalPlaySec;
        if (evt.numRounds > 0) {
            totalPlaySec = (unsigned long)evt.numRounds * roundDurationSec;
        } else {
            // Continuous mode — use the longer of booking duration or timer duration
            unsigned long bookingDuration = (evt.endTime > evt.startTime)
                ? (unsigned long)(evt.endTime - evt.startTime) : 0;
            totalPlaySec = (bookingDuration > roundDurationSec)
                ? bookingDuration : roundDurationSec;
        }

        if ((unsigned long)elapsed >= totalPlaySec) {
            remoteLog("Recovery skip \"%s\": all rounds done (elapsed=%lds total=%lus)",
                      evt.name.c_str(), (long)elapsed, totalPlaySec);
            continue; // All rounds finished
        }

        // Pick the event with the latest startTime (most relevant)
        if (!bestEvent || evt.startTime > bestEvent->startTime) {
            bestEvent = &evt;
        }
    }

    if (!bestEvent) {
        return result;
    }

    time_t elapsed = now - bestEvent->startTime;
    unsigned long roundDurationSec = (unsigned long)bestEvent->durationMin * 60;
    unsigned int currentRound = (unsigned int)(elapsed / roundDurationSec) + 1;
    unsigned long remainingInRoundSec = roundDurationSec - (elapsed % roundDurationSec);

    result.shouldRecover = true;
    result.eventId = bestEvent->id;
    result.eventName = bestEvent->name;
    result.durationMin = bestEvent->durationMin;
    result.numRounds = bestEvent->numRounds;
    result.currentRound = currentRound;
    result.remainingMs = remainingInRoundSec * 1000UL;
    result.eventEndTime = bestEvent->endTime;
    result.eventStartTime = bestEvent->startTime;

    DEBUG_PRINTF("Recovery check: event '%s' round %d, %lu sec remaining\n",
                 bestEvent->name.c_str(), currentRound, remainingInRoundSec);

    return result;
}

CachedEvent* HelloClubClient::checkAutoTrigger(Timezone& tz) {
    time_t now = UTC.now();

    for (auto& evt : events) {
        if (evt.triggered) {
            remoteLog("HC evt \"%s\": already triggered", evt.name.c_str());
            continue;
        }

        long delta = (long)(now - evt.startTime);
        long windowSec = HELLOCLUB_TRIGGER_WINDOW_MS / 1000;

        // Check if now is within the trigger window (startTime to startTime + 2 min)
        if (now >= evt.startTime && now <= evt.startTime + windowSec) {
            remoteLog("HC evt \"%s\": IN WINDOW delta=%lds", evt.name.c_str(), delta);
            return &evt;
        } else {
            remoteLog("HC evt \"%s\": start=%ld now=%ld delta=%lds",
                      evt.name.c_str(), (long)evt.startTime, (long)now, delta);
        }
    }

    return nullptr;
}

void HelloClubClient::markTriggered(const String& id, time_t startTime) {
    for (auto& evt : events) {
        if (evt.id == id && evt.startTime == startTime) {
            evt.triggered = true;
            DEBUG_PRINTF("HelloClub: Marked event '%s' as triggered\n", evt.name.c_str());
            saveToNVS();
            return;
        }
    }
}

void HelloClubClient::clearAllTriggered() {
    for (auto& evt : events) {
        evt.triggered = false;
    }
    saveToNVS();
}

void HelloClubClient::purgeExpired(Timezone& tz) {
    time_t now = UTC.now();
    size_t before = events.size();

    // Keep events for 5 minutes after their endTime if they haven't been triggered yet.
    // This handles short/instant bookings where endTime ≈ startTime — without this
    // grace period, the event gets purged before checkAutoTrigger can see it.
    const time_t PURGE_GRACE_SEC = 300; // 5 minutes

    events.erase(
        std::remove_if(events.begin(), events.end(),
            [now, PURGE_GRACE_SEC](const CachedEvent& evt) {
                // For triggered events, calculate actual play end time
                // (short bookings may have endTime << actual play time)
                if (evt.triggered) {
                    time_t playEnd = evt.startTime;
                    if (evt.numRounds > 0) {
                        playEnd += (time_t)evt.numRounds * evt.durationMin * 60;
                    } else {
                        playEnd += (time_t)evt.durationMin * 60;
                    }
                    time_t effectiveEnd = (evt.endTime > playEnd) ? evt.endTime : playEnd;
                    return effectiveEnd + PURGE_GRACE_SEC < now;
                }
                // Not triggered — keep for grace period after endTime
                return evt.endTime + PURGE_GRACE_SEC < now;
            }),
        events.end()
    );

    if (events.size() < before) {
        DEBUG_PRINTF("HelloClub: Purged %d expired events\n", before - events.size());
        saveToNVS();
    }
}
