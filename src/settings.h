#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "timer.h"
#include "siren.h"

class Settings {
public:
    Settings();

    bool load(Timer& timer, Siren& siren);
    bool save(const Timer& timer, const Siren& siren);
    bool clear();

    String getTimezone() const { return timezone; }
    bool setTimezone(const String& tz);

    // Hello Club default round duration
    uint16_t getHcDefaultDuration() const { return hcDefaultDuration; }
    bool setHcDefaultDuration(uint16_t minutes);

    // QR code settings
    String getGuestWifiPass() const { return guestWifiPass; }
    String getGuestWifiEnc() const { return guestWifiEnc; }
    String getGuestWifiSsid() const { return guestWifiSsid; }
    bool saveQrSettings(const String& pass, const String& enc, const String& ssid);

private:
    Preferences preferences;
    String timezone;
    uint16_t hcDefaultDuration;
    String guestWifiPass;
    String guestWifiEnc;
    String guestWifiSsid;

    void loadTimezone();
    void saveTimezone();
    void loadQrSettings();
};
