#include "timer.h"
#include "config.h"

Timer::Timer()
    : state(IDLE)
    , gameDuration(DEFAULT_GAME_DURATION)
    , numRounds(DEFAULT_NUM_ROUNDS)
    , currentRound(1)
    , mainTimerStart(0)
    , mainTimerRemaining(0)
    , pauseAfterNext(false)
    , continuousMode(false)
    , stateChanged(false)
    , roundEnded(false)
{
}

bool Timer::update() {
    stateChanged = false;
    roundEnded = false;

    if (state != RUNNING) {
        return false;
    }

    unsigned long now = millis();
    unsigned long mainElapsed = calculateElapsed(mainTimerStart, now);

    mainTimerRemaining = (mainElapsed < gameDuration) ? (gameDuration - mainElapsed) : 0;

    if (mainTimerRemaining == 0) {
        roundEnded = true;
        stateChanged = true;

        if (currentRound >= numRounds && !continuousMode) {
            state = FINISHED;
        } else if (pauseAfterNext) {
            // Siren fires (handled by caller) THEN pause
            currentRound++;
            mainTimerRemaining = gameDuration;  // Load next round
            state = PAUSED;                     // But don't start it
            pauseAfterNext = false;             // One-shot, auto-clear
        } else {
            currentRound++;
            mainTimerStart = millis();
        }
    }

    return stateChanged;
}

void Timer::start() {
    if (state == IDLE || state == FINISHED) {
        state = RUNNING;
        currentRound = 1;
        mainTimerStart = millis();
        mainTimerRemaining = gameDuration;
        pauseAfterNext = false;
    }
}

void Timer::startMidRound(unsigned int round, unsigned long remainingMs) {
    state = RUNNING;
    currentRound = round;
    mainTimerRemaining = remainingMs;
    pauseAfterNext = false;

    // Back-calculate mainTimerStart so update() computes correct remaining
    unsigned long elapsed = gameDuration - remainingMs;
    unsigned long now = millis();
    if (now >= elapsed) {
        mainTimerStart = now - elapsed;
    } else {
        // Handle millis() wrap (same pattern as resume())
        mainTimerStart = (0xFFFFFFFF - elapsed) + now + 1;
    }
}

void Timer::pause() {
    if (state == RUNNING) {
        state = PAUSED;
        unsigned long now = millis();
        unsigned long mainElapsed = calculateElapsed(mainTimerStart, now);
        mainTimerRemaining = (mainElapsed < gameDuration) ? (gameDuration - mainElapsed) : 0;
    }
}

void Timer::resume() {
    if (state == PAUSED) {
        state = RUNNING;
        unsigned long now = millis();
        unsigned long mainElapsed = gameDuration - mainTimerRemaining;

        if (now >= mainElapsed) {
            mainTimerStart = now - mainElapsed;
        } else {
            mainTimerStart = (0xFFFFFFFF - mainElapsed) + now + 1;
        }
    }
}

void Timer::reset() {
    state = IDLE;
    currentRound = 1;
    mainTimerRemaining = 0;
    pauseAfterNext = false;
    continuousMode = false;
}

bool Timer::hasRoundEnded() {
    return roundEnded;
}

bool Timer::isMatchFinished() {
    return state == FINISHED;
}

unsigned long Timer::calculateElapsed(unsigned long start, unsigned long now) {
    if (now >= start) {
        return now - start;
    } else {
        return (0xFFFFFFFF - start) + now + 1;
    }
}
