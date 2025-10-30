#include "helloclub.h"
#include <WiFiClientSecure.h>
#include <time.h>

// Let's Encrypt Root CA Certificate (ISRG Root X1)
// Valid until 2035-06-04
// This root cert covers most major APIs including helloclub.com
const char* rootCACertificate = \
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";

HelloClubClient::HelloClubClient() {
    apiKey = "";
    lastError = "";
}

void HelloClubClient::setApiKey(const String& key) {
    apiKey = key;
}

String HelloClubClient::getLastError() const {
    return lastError;
}

bool HelloClubClient::makeRequest(const String& endpoint, const String& params, DynamicJsonDocument& responseDoc) {
    if (apiKey.isEmpty()) {
        lastError = "API key not configured";
        return false;
    }

    const int MAX_RETRIES = 3;
    const int RETRY_DELAYS[] = {1000, 2000, 4000}; // Exponential backoff

    String url = baseUrl + endpoint;
    if (!params.isEmpty()) {
        url += "?" + params;
    }

    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        Serial.printf("HelloClub API Request (attempt %d/%d): %s\n", attempt + 1, MAX_RETRIES, url.c_str());

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

            Serial.printf("HelloClub API: Success on attempt %d\n", attempt + 1);
            return true;
        }

        http.end();

        // Check if retryable
        bool shouldRetry = (httpCode == 429 || httpCode == 503 || httpCode == 504 || httpCode < 0);

        lastError = "HTTP error: " + String(httpCode);
        if (httpCode == 401) {
            lastError += " (Invalid API key)";
            shouldRetry = false;
        } else if (httpCode == 429) {
            lastError += " (Rate limit exceeded)";
        }

        if (!shouldRetry || attempt == MAX_RETRIES - 1) {
            Serial.printf("HelloClub API: Failed - %s\n", lastError.c_str());
            return false;
        }

        Serial.printf("HelloClub API: Retrying in %dms...\n", RETRY_DELAYS[attempt]);
        delay(RETRY_DELAYS[attempt]);
    }

    return false;
}

bool HelloClubClient::fetchEvents(int daysAhead, const String& categoryFilter, std::vector<HelloClubEvent>& events) {
    events.clear();
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

    // Build request parameters
    String params = "fromDate=" + String(fromDate);
    params += "&toDate=" + String(toDate);
    params += "&sort=startDate";
    params += "&limit=50"; // Limit to 50 events to avoid memory issues

    // Make API request
    DynamicJsonDocument responseDoc(16384); // 16KB buffer for response
    if (!makeRequest("/event", params, responseDoc)) {
        return false;
    }

    // Parse events
    JsonArray eventsArray = responseDoc["events"].as<JsonArray>();
    if (eventsArray.isNull()) {
        lastError = "No events array in response";
        return false;
    }

    Serial.print("HelloClub: Found ");
    Serial.print(eventsArray.size());
    Serial.println(" events");

    for (JsonObject eventObj : eventsArray) {
        HelloClubEvent event;

        event.id = eventObj["id"].as<String>();
        event.name = eventObj["name"].as<String>();
        event.startDate = eventObj["startDate"].as<String>();
        event.endDate = eventObj["endDate"].as<String>();

        // Extract activity name
        if (eventObj.containsKey("activity") && eventObj["activity"].is<JsonObject>()) {
            event.activityName = eventObj["activity"]["name"].as<String>();
        }

        // Extract first category name
        if (eventObj.containsKey("categories") && eventObj["categories"].is<JsonArray>()) {
            JsonArray categories = eventObj["categories"].as<JsonArray>();
            if (categories.size() > 0 && categories[0].is<JsonObject>()) {
                event.categoryName = categories[0]["name"].as<String>();
            }
        }

        // Calculate duration
        event.durationMinutes = calculateDuration(event.startDate, event.endDate);

        // Apply category filter
        if (categoryFilter.isEmpty() || matchesCategory(event.categoryName, categoryFilter)) {
            events.push_back(event);
            Serial.print("  - ");
            Serial.print(event.name);
            Serial.print(" (");
            Serial.print(event.categoryName);
            Serial.println(")");
        }
    }

    Serial.print("HelloClub: Filtered to ");
    Serial.print(events.size());
    Serial.println(" events");

    return true;
}

bool HelloClubClient::fetchAvailableCategories(int daysAhead, std::vector<String>& categories) {
    categories.clear();

    std::vector<HelloClubEvent> events;
    if (!fetchEvents(daysAhead, "", events)) {
        return false;
    }

    // Extract unique categories
    for (const auto& event : events) {
        if (!event.categoryName.isEmpty()) {
            // Check if category already in list
            bool found = false;
            for (const auto& cat : categories) {
                if (cat.equalsIgnoreCase(event.categoryName)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                categories.push_back(event.categoryName);
            }
        }
    }

    // Sort categories alphabetically
    std::sort(categories.begin(), categories.end(), [](const String& a, const String& b) {
        return a.compareTo(b) < 0;
    });

    return true;
}

bool HelloClubClient::parseISODate(const String& isoDate, int& dayOfWeek, int& hour, int& minute) {
    // Expected format: 2025-10-30T18:00:00Z or 2025-10-30T18:00:00+00:00

    if (isoDate.length() < 19) {
        return false;
    }

    // Parse date components
    int year = isoDate.substring(0, 4).toInt();
    int month = isoDate.substring(5, 7).toInt();
    int day = isoDate.substring(8, 10).toInt();
    hour = isoDate.substring(11, 13).toInt();
    minute = isoDate.substring(14, 16).toInt();

    // Calculate day of week using Zeller's congruence
    if (month < 3) {
        month += 12;
        year--;
    }

    int q = day;
    int m = month;
    int k = year % 100;
    int j = year / 100;

    int h = (q + ((13 * (m + 1)) / 5) + k + (k / 4) + (j / 4) - (2 * j)) % 7;

    // Convert Zeller result to our format (0=Sunday, 6=Saturday)
    // Zeller: 0=Saturday, 1=Sunday, 2=Monday, ..., 6=Friday
    dayOfWeek = (h + 6) % 7;

    return true;
}

int HelloClubClient::calculateDuration(const String& startDate, const String& endDate) {
    // Simple duration calculation based on time difference
    // Parse hours and minutes from both dates

    if (startDate.length() < 19 || endDate.length() < 19) {
        return 60; // Default to 60 minutes if parsing fails
    }

    int startHour = startDate.substring(11, 13).toInt();
    int startMinute = startDate.substring(14, 16).toInt();
    int endHour = endDate.substring(11, 13).toInt();
    int endMinute = endDate.substring(14, 16).toInt();

    int startTotalMinutes = startHour * 60 + startMinute;
    int endTotalMinutes = endHour * 60 + endMinute;

    int duration = endTotalMinutes - startTotalMinutes;

    // Handle day boundary crossing
    if (duration < 0) {
        duration += 24 * 60; // Add 24 hours
    }

    // Sanity check
    if (duration <= 0 || duration > 24 * 60) {
        return 60; // Default to 60 minutes
    }

    return duration;
}

bool HelloClubClient::matchesCategory(const String& categoryName, const String& filterList) {
    if (filterList.isEmpty()) {
        return true; // No filter = match all
    }

    // Split filter list by comma
    int startPos = 0;
    while (startPos < filterList.length()) {
        int commaPos = filterList.indexOf(',', startPos);
        if (commaPos == -1) {
            commaPos = filterList.length();
        }

        String filter = filterList.substring(startPos, commaPos);
        filter.trim();

        if (categoryName.equalsIgnoreCase(filter)) {
            return true;
        }

        startPos = commaPos + 1;
    }

    return false;
}

bool HelloClubClient::convertEventToSchedule(const HelloClubEvent& event, const String& ownerUsername, Schedule& schedule, Timezone* localTz) {
    // Parse ISO 8601 date string (format: 2025-10-30T18:00:00Z or 2025-10-30T18:00:00+00:00)
    if (event.startDate.length() < 19) {
        lastError = "Invalid date format (too short): " + event.startDate;
        return false;
    }

    // Extract date/time components from ISO string
    int year = event.startDate.substring(0, 4).toInt();
    int month = event.startDate.substring(5, 7).toInt();
    int day = event.startDate.substring(8, 10).toInt();
    int hour = event.startDate.substring(11, 13).toInt();
    int minute = event.startDate.substring(14, 16).toInt();
    int second = event.startDate.substring(17, 19).toInt();

    // If timezone provided, convert from UTC to local time
    if (localTz != nullptr) {
        // Create UTC timestamp
        time_t utcTime = makeTime(hour, minute, second, day, month, year);

        // Convert to local time using ezTime
        // setTime sets the UTC time, then we get local time from timezone
        UTC.setTime(utcTime);
        time_t localTime = localTz->tzTime(utcTime);

        // Extract local time components
        tmElements_t tm;
        breakTime(localTime, tm);

        schedule.dayOfWeek = tm.Wday - 1; // ezTime: 1=Sunday, convert to 0=Sunday
        schedule.startHour = tm.Hour;
        schedule.startMinute = tm.Minute;

        Serial.printf("HelloClub: Converted %s UTC to local: Day=%d, %02d:%02d\n",
                      event.startDate.c_str(), schedule.dayOfWeek, schedule.startHour, schedule.startMinute);
    } else {
        // No timezone conversion - use UTC time directly (fallback)
        int dayOfWeek, utcHour, utcMinute;
        if (!parseISODate(event.startDate, dayOfWeek, utcHour, utcMinute)) {
            lastError = "Failed to parse event start date: " + event.startDate;
            return false;
        }

        schedule.dayOfWeek = dayOfWeek;
        schedule.startHour = utcHour;
        schedule.startMinute = utcMinute;

        Serial.println("Warning: No timezone provided for Hello Club conversion, using UTC times");
    }

    schedule.id = "hc-" + event.id.substring(0, 8); // Prefix with "hc-" for HelloClub
    schedule.clubName = event.name;
    schedule.ownerUsername = ownerUsername;
    schedule.durationMinutes = event.durationMinutes;
    schedule.enabled = true;
    schedule.createdAt = millis();

    return true;
}
