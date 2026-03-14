#include "settings.h"
#include "config.h"

Settings::Settings()
    : timezone(TIMEZONE_LOCATION)
    , hcDefaultDuration(DEFAULT_GAME_DURATION / 60000)
    , guestWifiPass("")
    , guestWifiEnc("WPA")
    , guestWifiSsid("")
{
    loadTimezone();
    loadQrSettings();

    // Load HC default duration
    if (preferences.begin(PREFERENCES_NAMESPACE, true)) {
        hcDefaultDuration = preferences.getUShort(PREF_KEY_HC_DEFAULT_DURATION, DEFAULT_GAME_DURATION / 60000);
        preferences.end();
    }
}

bool Settings::load(Timer& timer, Siren& siren) {
    if (!preferences.begin(PREFERENCES_NAMESPACE, true)) {
        DEBUG_PRINTLN("Failed to open preferences for reading. Using defaults.");
        return false;
    }

    timer.setGameDuration(preferences.getULong(PREF_KEY_GAME_DURATION, DEFAULT_GAME_DURATION));
    timer.setNumRounds(preferences.getUInt(PREF_KEY_NUM_ROUNDS, DEFAULT_NUM_ROUNDS));

    siren.setBlastLength(preferences.getULong(PREF_KEY_SIREN_LENGTH, DEFAULT_SIREN_LENGTH));
    siren.setBlastPause(preferences.getULong(PREF_KEY_SIREN_PAUSE, DEFAULT_SIREN_PAUSE));

    preferences.end();

    DEBUG_PRINTLN("Settings loaded successfully");
    DEBUG_PRINTF("  Game duration: %lu ms\n", timer.getGameDuration());
    DEBUG_PRINTF("  Num rounds: %u\n", timer.getNumRounds());
    DEBUG_PRINTF("  Siren length: %lu ms\n", siren.getBlastLength());
    DEBUG_PRINTF("  Siren pause: %lu ms\n", siren.getBlastPause());

    return true;
}

bool Settings::save(const Timer& timer, const Siren& siren) {
    if (!preferences.begin(PREFERENCES_NAMESPACE, false)) {
        DEBUG_PRINTLN("Failed to open preferences for writing.");
        return false;
    }

    preferences.putULong(PREF_KEY_GAME_DURATION, timer.getGameDuration());
    preferences.putUInt(PREF_KEY_NUM_ROUNDS, timer.getNumRounds());

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
    if (!preferences.begin(PREFERENCES_NAMESPACE, true)) {
        DEBUG_PRINTLN("Failed to open preferences for reading timezone. Using default.");
        timezone = TIMEZONE_LOCATION;
        return;
    }

    timezone = preferences.getString("timezone", TIMEZONE_LOCATION);
    preferences.end();

    DEBUG_PRINTF("Timezone loaded: %s\n", timezone.c_str());
}

void Settings::saveTimezone() {
    if (!preferences.begin(PREFERENCES_NAMESPACE, false)) {
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

    if (tz.indexOf('/') == -1) {
        DEBUG_PRINTLN("Invalid timezone format: must be IANA format (e.g., 'Region/City')");
        return false;
    }

    if (tz.length() < 3 || tz.length() > 50) {
        DEBUG_PRINTLN("Invalid timezone: length out of range");
        return false;
    }

    timezone = tz;
    saveTimezone();

    DEBUG_PRINTF("Timezone set to: %s\n", timezone.c_str());
    return true;
}

bool Settings::setHcDefaultDuration(uint16_t minutes) {
    if (minutes < 1 || minutes > 120) return false;
    hcDefaultDuration = minutes;

    if (!preferences.begin(PREFERENCES_NAMESPACE, false)) return false;
    preferences.putUShort(PREF_KEY_HC_DEFAULT_DURATION, hcDefaultDuration);
    preferences.end();

    DEBUG_PRINTF("HC default duration set to: %d min\n", hcDefaultDuration);
    return true;
}

void Settings::loadQrSettings() {
    if (!preferences.begin(PREFERENCES_NAMESPACE, true)) {
        return;
    }

    guestWifiPass = preferences.getString("guestWifiPass", "");
    guestWifiEnc = preferences.getString("guestWifiEnc", "WPA");
    guestWifiSsid = preferences.getString("guestWifiSsid", "");
    preferences.end();
}

bool Settings::saveQrSettings(const String& pass, const String& enc, const String& ssid) {
    if (!preferences.begin(PREFERENCES_NAMESPACE, false)) {
        DEBUG_PRINTLN("Failed to open preferences for writing QR settings.");
        return false;
    }

    guestWifiPass = pass;
    guestWifiEnc = enc;
    guestWifiSsid = ssid;

    preferences.putString("guestWifiPass", guestWifiPass);
    preferences.putString("guestWifiEnc", guestWifiEnc);
    preferences.putString("guestWifiSsid", guestWifiSsid);
    preferences.end();

    DEBUG_PRINTLN("QR settings saved");
    return true;
}
