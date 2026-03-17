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
        // Safety: even if not active, ensure relay is off
        // (protects against state corruption or missed stop())
        if (relayOn) {
            digitalWrite(relayPin, LOW);
            relayOn = false;
            DEBUG_PRINTLN("Siren safety: relay forced off (inactive but relayOn)");
        }
        return;
    }

    unsigned long now = millis();

    // Safety timeout: if relay has been on for way too long (e.g. loop was blocked),
    // force it off immediately. This prevents the siren running continuously
    // if something stalls the main loop.
    if (relayOn && (now - lastActionTime >= SAFETY_TIMEOUT_MS)) {
        DEBUG_PRINTF("Siren safety timeout: relay was on for %lu ms, forcing off\n",
                     now - lastActionTime);
        digitalWrite(relayPin, LOW);
        relayOn = false;
        lastActionTime = now;
        blastsRemaining--;
        if (blastsRemaining <= 0) {
            active = false;
            DEBUG_PRINTLN("Siren sequence complete (after safety timeout)");
        }
        return;
    }

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
