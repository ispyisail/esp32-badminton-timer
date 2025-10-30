#pragma once

#include <Arduino.h>

/**
 * @brief Siren control module for non-blocking relay control
 *
 * This class manages the relay/siren with a non-blocking state machine,
 * allowing for multiple blasts with configurable timing without using delay().
 */
class Siren {
public:
    /**
     * @brief Constructor
     * @param pin GPIO pin connected to relay
     */
    Siren(int pin);

    /**
     * @brief Initialize the siren (set pin mode)
     */
    void begin();

    /**
     * @brief Update siren state (call every loop iteration)
     */
    void update();

    /**
     * @brief Start a siren sequence
     * @param blasts Number of times the siren should sound
     */
    void start(int blasts);

    /**
     * @brief Check if siren is currently active
     * @return True if siren sequence is running
     */
    bool isActive() const { return active; }

    /**
     * @brief Set siren blast length
     * @param length Length in milliseconds
     */
    void setBlastLength(unsigned long length) { blastLength = length; }

    /**
     * @brief Set pause between blasts
     * @param pause Pause duration in milliseconds
     */
    void setBlastPause(unsigned long pause) { blastPause = pause; }

    /**
     * @brief Get current blast length setting
     */
    unsigned long getBlastLength() const { return blastLength; }

    /**
     * @brief Get current blast pause setting
     */
    unsigned long getBlastPause() const { return blastPause; }

    /**
     * @brief Stop siren immediately
     */
    void stop();

private:
    int relayPin;
    unsigned long blastLength;
    unsigned long blastPause;

    // State machine variables
    bool active;
    int blastsRemaining;
    unsigned long lastActionTime;
    bool relayOn;
};
