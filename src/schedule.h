#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include <ezTime.h>

/**
 * @brief Schedule entry for automatic timer control
 */
struct Schedule {
    String id;              // Unique ID (generated from timestamp)
    String clubName;        // Club name (used for grouping and access control)
    String ownerUsername;   // Username of operator who created this
    int dayOfWeek;          // 0=Sunday, 1=Monday, ..., 6=Saturday
    int startHour;          // 0-23
    int startMinute;        // 0-59
    int durationMinutes;    // Session duration in minutes
    bool enabled;           // Whether this schedule is active
    unsigned long createdAt; // Timestamp when created
};

/**
 * @brief Schedule manager for automatic timer control
 *
 * Manages recurring weekly schedules for different clubs.
 * Operators can create schedules for their club only.
 * Admin can manage all schedules.
 */
class ScheduleManager {
public:
    /**
     * @brief Constructor
     */
    ScheduleManager();

    /**
     * @brief Initialize schedule manager and load from NVS
     */
    void begin();

    /**
     * @brief Add a new schedule
     * @param schedule Schedule to add
     * @return True if added successfully
     */
    bool addSchedule(const Schedule& schedule);

    /**
     * @brief Update an existing schedule
     * @param schedule Schedule with updated data
     * @return True if updated successfully
     */
    bool updateSchedule(const Schedule& schedule);

    /**
     * @brief Delete a schedule
     * @param id Schedule ID to delete
     * @return True if deleted successfully
     */
    bool deleteSchedule(const String& id);

    /**
     * @brief Get all schedules
     * @return Vector of all schedules
     */
    std::vector<Schedule> getAllSchedules();

    /**
     * @brief Get schedules for a specific club
     * @param clubName Club name to filter by
     * @return Vector of schedules for that club
     */
    std::vector<Schedule> getSchedulesByClub(const String& clubName);

    /**
     * @brief Get schedule by ID
     * @param id Schedule ID
     * @param schedule Output parameter for schedule
     * @return True if found
     */
    bool getScheduleById(const String& id, Schedule& schedule);

    /**
     * @brief Check if any schedule should trigger now
     * @param tz Timezone object for current time
     * @param triggeredSchedule Output parameter for triggered schedule
     * @return True if a schedule should trigger
     */
    bool checkScheduleTrigger(Timezone& tz, Schedule& triggeredSchedule);

    /**
     * @brief Mark a schedule as triggered (to prevent re-triggering)
     * @param id Schedule ID
     * @param triggerTime Time it was triggered (minute resolution)
     */
    void markTriggered(const String& id, unsigned long triggerTime);

    /**
     * @brief Check if user has permission to modify schedule
     * @param schedule Schedule to check
     * @param username Username attempting modification
     * @param isAdmin Whether user is admin
     * @return True if user has permission
     */
    bool hasPermission(const Schedule& schedule, const String& username, bool isAdmin);

    /**
     * @brief Enable or disable master schedule system
     * @param enabled Enable/disable
     */
    void setSchedulingEnabled(bool enabled);

    /**
     * @brief Check if scheduling is enabled
     */
    bool isSchedulingEnabled() const { return schedulingEnabled; }

    /**
     * @brief Generate a unique ID for a schedule
     */
    String generateScheduleId();

    /**
     * @brief Get current time in minutes since start of week
     * @param tz Timezone object
     * @return Minutes since Sunday 00:00
     */
    int getCurrentWeekMinute(Timezone& tz);

private:
    std::vector<Schedule> schedules;
    std::map<String, unsigned long> lastTriggerTimes; // Track when each schedule last triggered
    bool schedulingEnabled;
    unsigned int scheduleIdCounter; // Counter to ensure unique IDs

    static const int MAX_SCHEDULES = 50;
    static const char* PREF_NAMESPACE;
    static const char* PREF_SCHEDULE_COUNT;
    static const char* PREF_SCHEDULE_PREFIX;
    static const char* PREF_SCHEDULING_ENABLED;

    /**
     * @brief Load schedules from NVS
     */
    void load();

    /**
     * @brief Save schedules to NVS
     */
    void save();

    /**
     * @brief Get schedule time in minutes since start of week
     * @param schedule Schedule to convert
     * @return Minutes since Sunday 00:00
     */
    int getScheduleWeekMinute(const Schedule& schedule);
};
