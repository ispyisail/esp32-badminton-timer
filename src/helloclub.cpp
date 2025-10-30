#include "helloclub.h"
#include <WiFiClientSecure.h>
#include <time.h>

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

    WiFiClientSecure client;
    client.setInsecure(); // Note: For production, should verify certificate

    HTTPClient http;
    String url = baseUrl + endpoint;
    if (!params.isEmpty()) {
        url += "?" + params;
    }

    Serial.print("HelloClub API Request: ");
    Serial.println(url);

    if (!http.begin(client, url)) {
        lastError = "Failed to begin HTTP request";
        return false;
    }

    // Set headers
    http.addHeader("X-Api-Key", apiKey);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        lastError = "HTTP error: " + String(httpCode);
        if (httpCode == 401) {
            lastError += " (Invalid API key)";
        } else if (httpCode == 429) {
            lastError += " (Rate limit exceeded)";
        }
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    DeserializationError error = deserializeJson(responseDoc, payload);
    if (error) {
        lastError = "JSON parse error: " + String(error.c_str());
        return false;
    }

    return true;
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

bool HelloClubClient::convertEventToSchedule(const HelloClubEvent& event, const String& ownerUsername, Schedule& schedule) {
    // Parse start date to get day of week and time
    int dayOfWeek, hour, minute;
    if (!parseISODate(event.startDate, dayOfWeek, hour, minute)) {
        lastError = "Failed to parse event start date: " + event.startDate;
        return false;
    }

    // Convert to local timezone (Pacific/Auckland is UTC+12 or UTC+13 depending on DST)
    // For simplicity, we'll use the UTC time from the API
    // In production, you might want to adjust for timezone

    schedule.id = "hc-" + event.id.substring(0, 8); // Prefix with "hc-" for HelloClub
    schedule.clubName = event.name;
    schedule.ownerUsername = ownerUsername;
    schedule.dayOfWeek = dayOfWeek;
    schedule.startHour = hour;
    schedule.startMinute = minute;
    schedule.durationMinutes = event.durationMinutes;
    schedule.enabled = true;
    schedule.createdAt = millis();

    return true;
}
