#pragma once

#include <Preferences.h>
#include "timer.h"
#include "siren.h"

/**
 * @brief Settings module for persistent storage using ESP32 Preferences (NVS)
 *
 * This class handles loading and saving timer settings to non-volatile memory.
 */
class Settings {
public:
    /**
     * @brief Constructor
     */
    Settings();

    /**
     * @brief Load settings from NVS and apply to timer and siren
     * @param timer Timer instance to configure
     * @param siren Siren instance to configure
     * @return True if load successful, false otherwise
     */
    bool load(Timer& timer, Siren& siren);

    /**
     * @brief Save current timer and siren settings to NVS
     * @param timer Timer instance to save from
     * @param siren Siren instance to save from
     * @return True if save successful, false otherwise
     */
    bool save(const Timer& timer, const Siren& siren);

    /**
     * @brief Clear all settings from NVS
     * @return True if clear successful, false otherwise
     */
    bool clear();

private:
    Preferences preferences;
};
