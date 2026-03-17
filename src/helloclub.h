#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>
#include <ezTime.h>

// Cached event from Hello Club API
struct CachedEvent {
    String id;              // HC event ID (first 12 chars)
    String name;            // Event name (max 40 chars)
    time_t startTime;       // UTC epoch
    time_t endTime;         // UTC epoch — from HC booking slot
    uint16_t durationMin;   // Game duration (from timer: tag or default)
    uint8_t numRounds;      // Rounds (from timer: tag or default)
    bool triggered;         // Already auto-started
};

// Result from mid-event boot recovery check
struct RecoveryResult {
    bool shouldRecover;
    String eventId;
    String eventName;
    uint16_t durationMin;
    uint8_t numRounds;          // 0 = continuous
    unsigned int currentRound;
    unsigned long remainingMs;  // milliseconds remaining in current round
    time_t eventEndTime;
};

class HelloClubClient {
public:
    HelloClubClient();

    void setApiKey(const String& apiKey);
    void setDefaults(uint16_t defaultDuration, uint8_t defaultRounds);

    // Fetch events with timer: tag from HC API, update cache
    // Returns true if fetch succeeded (cache updated)
    bool fetchAndCacheEvents(int daysAhead, Timezone& tz);

    // Load cached events from NVS (for boot without internet)
    void loadFromNVS();

    // Save cached events to NVS
    void saveToNVS();

    // Check if any event should auto-trigger now
    // Returns pointer to event if trigger should fire, nullptr otherwise
    CachedEvent* checkAutoTrigger(Timezone& tz);

    // Mark event as triggered and save
    void markTriggered(const String& id);

    // Check if we're mid-event after a reboot — returns recovery info
    RecoveryResult checkMidEventRecovery();

    // Purge expired events (endTime < now)
    void purgeExpired(Timezone& tz);

    // Get all cached events (for WebSocket broadcast)
    const std::vector<CachedEvent>& getCachedEvents() const { return events; }

    // Get last sync time (millis)
    unsigned long getLastSyncTime() const { return lastSyncTime; }

    // Get last error
    String getLastError() const { return lastError; }

    // Get total events from last API response (before timer: tag filtering)
    int getTotalEventsFromApi() const { return totalEventsFromApi; }

    // Get debug info from last sync (event names + descriptions for troubleshooting)
    String getLastSyncDebug() const { return lastSyncDebug; }

    // Check if API key is configured
    bool isConfigured() const { return !apiKey.isEmpty(); }

private:
    String apiKey;
    String lastError;
    unsigned long lastSyncTime;
    uint16_t defaultDurationMin;
    uint8_t defaultNumRounds;
    int totalEventsFromApi = 0;
    String lastSyncDebug;
    std::vector<CachedEvent> events;

    static const int HC_MAX_EVENTS = 20;
    static const char* NVS_NAMESPACE;
    static const char* NVS_EVENTS_KEY;

    const String baseUrl = "https://api.helloclub.com";

    // Make HTTP request with retry and JSON filter
    bool makeRequest(const String& endpoint, const String& params,
                     DynamicJsonDocument& responseDoc, const JsonDocument& filter);

    // Parse timer: tag from event description
    // Returns true if timer: tag found; sets duration and rounds
    bool parseTimerTag(const String& description, uint16_t& duration, uint8_t& rounds);

    // Parse ISO 8601 date to time_t (UTC epoch)
    time_t parseISOToEpoch(const String& isoDate);
};
