#include "settings.h"
#include "config.h"

Settings::Settings()
    : timezone(TIMEZONE_LOCATION) // Default from config
{
    loadTimezone();
}

bool Settings::load(Timer& timer, Siren& siren) {
    if (!preferences.begin(PREFERENCES_NAMESPACE, true)) { // Read-only mode
        DEBUG_PRINTLN("Failed to open preferences for reading. Using defaults.");
        return false;
    }

    // Load timer settings
    timer.setGameDuration(preferences.getULong(PREF_KEY_GAME_DURATION, DEFAULT_GAME_DURATION));
    timer.setBreakDuration(preferences.getULong(PREF_KEY_BREAK_DURATION, DEFAULT_BREAK_DURATION));
    timer.setNumRounds(preferences.getUInt(PREF_KEY_NUM_ROUNDS, DEFAULT_NUM_ROUNDS));
    timer.setBreakTimerEnabled(preferences.getBool(PREF_KEY_BREAK_ENABLED, DEFAULT_BREAK_TIMER_ENABLED));

    // Load siren settings
    siren.setBlastLength(preferences.getULong(PREF_KEY_SIREN_LENGTH, DEFAULT_SIREN_LENGTH));
    siren.setBlastPause(preferences.getULong(PREF_KEY_SIREN_PAUSE, DEFAULT_SIREN_PAUSE));

    preferences.end();

    DEBUG_PRINTLN("Settings loaded successfully");
    DEBUG_PRINTF("  Game duration: %lu ms\n", timer.getGameDuration());
    DEBUG_PRINTF("  Break duration: %lu ms\n", timer.getBreakDuration());
    DEBUG_PRINTF("  Num rounds: %u\n", timer.getNumRounds());
    DEBUG_PRINTF("  Break timer enabled: %d\n", timer.isBreakTimerEnabled());
    DEBUG_PRINTF("  Siren length: %lu ms\n", siren.getBlastLength());
    DEBUG_PRINTF("  Siren pause: %lu ms\n", siren.getBlastPause());

    return true;
}

bool Settings::save(const Timer& timer, const Siren& siren) {
    if (!preferences.begin(PREFERENCES_NAMESPACE, false)) { // Read-write mode
        DEBUG_PRINTLN("Failed to open preferences for writing.");
        return false;
    }

    // Save timer settings
    preferences.putULong(PREF_KEY_GAME_DURATION, timer.getGameDuration());
    preferences.putULong(PREF_KEY_BREAK_DURATION, timer.getBreakDuration());
    preferences.putUInt(PREF_KEY_NUM_ROUNDS, timer.getNumRounds());
    preferences.putBool(PREF_KEY_BREAK_ENABLED, timer.isBreakTimerEnabled());

    // Save siren settings
    preferences.putULong(PREF_KEY_SIREN_LENGTH, siren.getBlastLength());
    preferences.putULong(PREF_KEY_SIREN_PAUSE, siren.getBlastPause());

    preferences.end();

    DEBUG_PRINTLN("Settings saved successfully");
    return true;
}

bool Settings::clear() {
    if (!preferences.begin(PREFERENCES_NAMESPACE, false)) {
        DEBUG_PRINTLN("Failed to open preferences for clearing.");
        return false;
    }

    bool result = preferences.clear();
    preferences.end();

    if (result) {
        DEBUG_PRINTLN("Settings cleared successfully");
    } else {
        DEBUG_PRINTLN("Failed to clear settings");
    }

    return result;
}

void Settings::loadTimezone() {
    if (!preferences.begin(PREFERENCES_NAMESPACE, true)) { // Read-only
        DEBUG_PRINTLN("Failed to open preferences for reading timezone. Using default.");
        timezone = TIMEZONE_LOCATION;
        return;
    }

    timezone = preferences.getString("timezone", TIMEZONE_LOCATION);
    preferences.end();

    DEBUG_PRINTF("Timezone loaded: %s\n", timezone.c_str());
}

void Settings::saveTimezone() {
    if (!preferences.begin(PREFERENCES_NAMESPACE, false)) { // Read-write
        DEBUG_PRINTLN("Failed to open preferences for writing timezone.");
        return;
    }

    preferences.putString("timezone", timezone);
    preferences.end();

    DEBUG_PRINTF("Timezone saved: %s\n", timezone.c_str());
}

bool Settings::setTimezone(const String& tz) {
    if (tz.length() == 0) {
        DEBUG_PRINTLN("Invalid timezone: empty string");
        return false;
    }

    timezone = tz;
    saveTimezone();

    DEBUG_PRINTF("Timezone set to: %s\n", timezone.c_str());
    return true;
}
