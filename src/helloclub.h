#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include "schedule.h"

/**
 * Hello Club Event Structure
 * Represents an event from the Hello Club API
 */
struct HelloClubEvent {
    String id;
    String name;
    String startDate;      // ISO 8601 format
    String endDate;        // ISO 8601 format
    String activityName;
    String categoryName;
    int durationMinutes;
};

/**
 * Hello Club API Client
 * Handles communication with Hello Club API for event management
 */
class HelloClubClient {
public:
    HelloClubClient();

    /**
     * Set the API key for Hello Club authentication
     * @param apiKey The API key from Hello Club settings
     */
    void setApiKey(const String& apiKey);

    /**
     * Fetch events from Hello Club within date range
     * @param daysAhead Number of days ahead to fetch events for
     * @param categoryFilter Comma-separated list of categories to filter (empty = all)
     * @param events Output vector to store fetched events
     * @return true if successful, false otherwise
     */
    bool fetchEvents(int daysAhead, const String& categoryFilter, std::vector<HelloClubEvent>& events);

    /**
     * Fetch all available categories from upcoming events
     * @param daysAhead Number of days ahead to scan for categories
     * @param categories Output vector to store unique category names
     * @return true if successful, false otherwise
     */
    bool fetchAvailableCategories(int daysAhead, std::vector<String>& categories);

    /**
     * Convert Hello Club event to Schedule struct
     * @param event The Hello Club event to convert
     * @param ownerUsername Username to assign as schedule owner
     * @param schedule Output schedule struct
     * @return true if conversion successful, false otherwise
     */
    bool convertEventToSchedule(const HelloClubEvent& event, const String& ownerUsername, Schedule& schedule);

    /**
     * Get last error message
     * @return Error message from last failed operation
     */
    String getLastError() const;

private:
    String apiKey;
    String lastError;
    const String baseUrl = "https://api.helloclub.com";

    /**
     * Make HTTP GET request to Hello Club API
     * @param endpoint API endpoint (e.g., "/event")
     * @param params URL parameters
     * @param responseDoc Output JSON document
     * @return true if successful, false otherwise
     */
    bool makeRequest(const String& endpoint, const String& params, DynamicJsonDocument& responseDoc);

    /**
     * Parse ISO 8601 date to day of week and time
     * @param isoDate ISO 8601 date string
     * @param dayOfWeek Output day of week (0=Sunday, 6=Saturday)
     * @param hour Output hour (0-23)
     * @param minute Output minute (0-59)
     * @return true if parsing successful, false otherwise
     */
    bool parseISODate(const String& isoDate, int& dayOfWeek, int& hour, int& minute);

    /**
     * Calculate duration between two ISO 8601 dates in minutes
     * @param startDate ISO 8601 start date
     * @param endDate ISO 8601 end date
     * @return Duration in minutes
     */
    int calculateDuration(const String& startDate, const String& endDate);

    /**
     * Check if event matches category filter
     * @param categoryName Category name from event
     * @param filterList Comma-separated list of categories
     * @return true if matches (or filter empty), false otherwise
     */
    bool matchesCategory(const String& categoryName, const String& filterList);
};
