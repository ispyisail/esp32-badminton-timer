#include "schedule.h"
#include <ArduinoJson.h>

// NVS keys
const char* ScheduleManager::PREF_NAMESPACE = "schedules";
const char* ScheduleManager::PREF_SCHEDULE_COUNT = "count";
const char* ScheduleManager::PREF_SCHEDULE_PREFIX = "sched_";
const char* ScheduleManager::PREF_SCHEDULING_ENABLED = "enabled";

ScheduleManager::ScheduleManager()
    : schedulingEnabled(false)
    , scheduleIdCounter(0)
{
}

void ScheduleManager::begin() {
    load();
}

void ScheduleManager::load() {
    Preferences prefs;
    if (!prefs.begin(PREF_NAMESPACE, true)) {
        Serial.println("Failed to open schedule preferences (read-only). Using defaults.");
        schedulingEnabled = false;
        return;
    }

    schedulingEnabled = prefs.getBool(PREF_SCHEDULING_ENABLED, false);
    int scheduleCount = prefs.getInt(PREF_SCHEDULE_COUNT, 0);

    schedules.clear();
    for (int i = 0; i < scheduleCount && i < MAX_SCHEDULES; i++) {
        String key = String(PREF_SCHEDULE_PREFIX) + String(i);
        String json = prefs.getString(key.c_str(), "");

        if (json.length() > 0) {
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, json);

            if (!error) {
                Schedule sched;
                sched.id = doc["id"].as<String>();
                sched.clubName = doc["club"].as<String>();
                sched.ownerUsername = doc["owner"].as<String>();
                sched.dayOfWeek = doc["day"].as<int>();
                sched.startHour = doc["hour"].as<int>();
                sched.startMinute = doc["minute"].as<int>();
                sched.durationMinutes = doc["duration"].as<int>();
                sched.enabled = doc["enabled"].as<bool>();
                sched.createdAt = doc["created"].as<unsigned long>();

                schedules.push_back(sched);
            }
        }
    }

    prefs.end();
    Serial.printf("Loaded %d schedule(s) from NVS. Scheduling %s\n",
                  schedules.size(), schedulingEnabled ? "ENABLED" : "DISABLED");
}

void ScheduleManager::save() {
    Preferences prefs;
    if (!prefs.begin(PREF_NAMESPACE, false)) {
        Serial.println("Failed to open schedule preferences for writing.");
        return;
    }

    prefs.putBool(PREF_SCHEDULING_ENABLED, schedulingEnabled);
    prefs.putInt(PREF_SCHEDULE_COUNT, schedules.size());

    for (size_t i = 0; i < schedules.size() && i < MAX_SCHEDULES; i++) {
        String key = String(PREF_SCHEDULE_PREFIX) + String(i);

        StaticJsonDocument<512> doc;
        doc["id"] = schedules[i].id;
        doc["club"] = schedules[i].clubName;
        doc["owner"] = schedules[i].ownerUsername;
        doc["day"] = schedules[i].dayOfWeek;
        doc["hour"] = schedules[i].startHour;
        doc["minute"] = schedules[i].startMinute;
        doc["duration"] = schedules[i].durationMinutes;
        doc["enabled"] = schedules[i].enabled;
        doc["created"] = schedules[i].createdAt;

        String json;
        serializeJson(doc, json);
        prefs.putString(key.c_str(), json);
    }

    prefs.end();
    Serial.printf("Saved %d schedule(s) to NVS\n", schedules.size());
}

bool ScheduleManager::addSchedule(const Schedule& schedule) {
    if (schedules.size() >= MAX_SCHEDULES) {
        Serial.println("Cannot add schedule: maximum limit reached");
        return false;
    }

    // Validate schedule
    if (schedule.dayOfWeek < 0 || schedule.dayOfWeek > 6) {
        Serial.println("Invalid day of week");
        return false;
    }
    if (schedule.startHour < 0 || schedule.startHour > 23) {
        Serial.println("Invalid hour");
        return false;
    }
    if (schedule.startMinute < 0 || schedule.startMinute > 59) {
        Serial.println("Invalid minute");
        return false;
    }
    if (schedule.durationMinutes < MIN_SCHEDULE_DURATION_MIN || schedule.durationMinutes > MAX_SCHEDULE_DURATION_MIN) {
        Serial.printf("Invalid duration (must be %d-%d minutes)\n", MIN_SCHEDULE_DURATION_MIN, MAX_SCHEDULE_DURATION_MIN);
        return false;
    }

    schedules.push_back(schedule);
    save();

    Serial.printf("Added schedule: %s for %s (Day %d, %02d:%02d, %d min)\n",
                  schedule.id.c_str(), schedule.clubName.c_str(),
                  schedule.dayOfWeek, schedule.startHour, schedule.startMinute,
                  schedule.durationMinutes);
    return true;
}

bool ScheduleManager::updateSchedule(const Schedule& schedule) {
    for (auto& sched : schedules) {
        if (sched.id == schedule.id) {
            sched = schedule;
            save();
            Serial.printf("Updated schedule: %s\n", schedule.id.c_str());
            return true;
        }
    }

    Serial.printf("Schedule not found: %s\n", schedule.id.c_str());
    return false;
}

bool ScheduleManager::deleteSchedule(const String& id) {
    for (auto it = schedules.begin(); it != schedules.end(); ++it) {
        if (it->id == id) {
            Serial.printf("Deleted schedule: %s\n", id.c_str());
            schedules.erase(it);

            // Clean up trigger tracking to prevent memory leak
            lastTriggerTimes.erase(id);

            save();
            return true;
        }
    }

    Serial.printf("Schedule not found: %s\n", id.c_str());
    return false;
}

std::vector<Schedule> ScheduleManager::getAllSchedules() {
    return schedules;
}

std::vector<Schedule> ScheduleManager::getSchedulesByClub(const String& clubName) {
    std::vector<Schedule> clubSchedules;
    for (const auto& sched : schedules) {
        if (sched.clubName == clubName) {
            clubSchedules.push_back(sched);
        }
    }
    return clubSchedules;
}

bool ScheduleManager::getScheduleById(const String& id, Schedule& schedule) {
    for (const auto& sched : schedules) {
        if (sched.id == id) {
            schedule = sched;
            return true;
        }
    }
    return false;
}

int ScheduleManager::getCurrentWeekMinute(Timezone& tz) {
    int dayOfWeek = tz.weekday() - 1; // ezTime: 1=Sunday, convert to 0=Sunday
    int hour = tz.hour();
    int minute = tz.minute();

    return (dayOfWeek * 24 * 60) + (hour * 60) + minute;
}

int ScheduleManager::getScheduleWeekMinute(const Schedule& schedule) {
    return (schedule.dayOfWeek * 24 * 60) + (schedule.startHour * 60) + schedule.startMinute;
}

bool ScheduleManager::checkScheduleTrigger(Timezone& tz, Schedule& triggeredSchedule) {
    if (!schedulingEnabled) {
        return false;
    }

    int currentWeekMinute = getCurrentWeekMinute(tz);

    for (const auto& sched : schedules) {
        if (!sched.enabled) {
            continue;
        }

        int scheduleWeekMinute = getScheduleWeekMinute(sched);

        // Check if current time matches schedule time (within same minute)
        if (currentWeekMinute == scheduleWeekMinute) {
            // Check if we already triggered this schedule recently (within last 2 minutes)
            if (lastTriggerTimes.find(sched.id) != lastTriggerTimes.end()) {
                unsigned long lastTrigger = lastTriggerTimes[sched.id];

                // Calculate difference with week wraparound handling
                // Week has 10080 minutes (7 * 24 * 60)
                const int WEEK_MINUTES = 10080;
                int timeSinceTrigger;

                if (currentWeekMinute >= lastTrigger) {
                    timeSinceTrigger = currentWeekMinute - lastTrigger;
                } else {
                    // Week wrapped around (e.g., Saturday 23:59 -> Sunday 00:00)
                    timeSinceTrigger = (WEEK_MINUTES - lastTrigger) + currentWeekMinute;
                }

                if (timeSinceTrigger < SCHEDULE_TRIGGER_DEBOUNCE_MIN) {
                    continue; // Don't re-trigger within debounce period
                }
            }

            // This schedule should trigger!
            triggeredSchedule = sched;
            return true;
        }
    }

    return false;
}

void ScheduleManager::markTriggered(const String& id, unsigned long triggerTime) {
    lastTriggerTimes[id] = triggerTime;
    Serial.printf("Marked schedule %s as triggered at minute %lu\n", id.c_str(), triggerTime);
}

bool ScheduleManager::hasPermission(const Schedule& schedule, const String& username, bool isAdmin) {
    // Admin can modify any schedule
    if (isAdmin) {
        return true;
    }

    // Operator can only modify their own club's schedules
    return schedule.ownerUsername == username;
}

void ScheduleManager::setSchedulingEnabled(bool enabled) {
    schedulingEnabled = enabled;
    save();
    Serial.printf("Scheduling system %s\n", enabled ? "ENABLED" : "DISABLED");
}

String ScheduleManager::generateScheduleId() {
    // Generate unique ID from timestamp + counter
    // Format: timestamp-counter (e.g., "123456789-1")
    unsigned long timestamp = millis();
    scheduleIdCounter++;

    String candidateId;
    bool isUnique = false;
    int attempts = 0;

    // Try up to 10 times to generate a unique ID
    while (!isUnique && attempts < 10) {
        candidateId = String(timestamp) + "-" + String(scheduleIdCounter + attempts);

        // Check if ID already exists
        isUnique = true;
        for (const auto& sched : schedules) {
            if (sched.id == candidateId) {
                isUnique = false;
                break;
            }
        }

        attempts++;
    }

    if (!isUnique) {
        // Fallback: add random component if collision still occurs
        candidateId = String(timestamp) + "-" + String(scheduleIdCounter) + "-" + String(random(10000, 99999));
    }

    return candidateId;
}
