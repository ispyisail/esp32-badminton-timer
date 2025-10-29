#include "siren.h"
#include "config.h"

Siren::Siren(int pin)
    : relayPin(pin)
    , blastLength(DEFAULT_SIREN_LENGTH)
    , blastPause(DEFAULT_SIREN_PAUSE)
    , active(false)
    , blastsRemaining(0)
    , lastActionTime(0)
    , relayOn(false)
{
}

void Siren::begin() {
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW);
    DEBUG_PRINTLN("Siren initialized");
}

void Siren::update() {
    if (!active) {
        return;
    }

    unsigned long now = millis();

    if (relayOn) {
        // Relay is currently on, check if blast duration has elapsed
        if (now - lastActionTime >= blastLength) {
            digitalWrite(relayPin, LOW);
            relayOn = false;
            lastActionTime = now;
            blastsRemaining--;

            if (blastsRemaining <= 0) {
                active = false;
                DEBUG_PRINTLN("Siren sequence complete");
            }
        }
    } else {
        // Relay is currently off
        if (blastsRemaining > 0) {
            // Pause between blasts
            if (now - lastActionTime >= blastPause) {
                digitalWrite(relayPin, HIGH);
                relayOn = true;
                lastActionTime = now;
                DEBUG_PRINTF("Siren blast %d\n", blastsRemaining);
            }
        } else {
            active = false;
        }
    }
}

void Siren::start(int blasts) {
    if (active || blasts <= 0) {
        return; // Don't start a new sequence if one is running or invalid blast count
    }

    DEBUG_PRINTF("Starting siren: %d blasts\n", blasts);
    blastsRemaining = blasts;
    active = true;
    relayOn = false;
    // Start the first blast immediately by pretending the last pause just ended
    lastActionTime = millis() - blastPause;
}

void Siren::stop() {
    active = false;
    blastsRemaining = 0;
    digitalWrite(relayPin, LOW);
    relayOn = false;
    DEBUG_PRINTLN("Siren stopped");
}
