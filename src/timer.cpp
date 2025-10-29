#include "timer.h"
#include "config.h"

Timer::Timer()
    : state(IDLE)
    , gameDuration(DEFAULT_GAME_DURATION)
    , breakDuration(DEFAULT_BREAK_DURATION)
    , numRounds(DEFAULT_NUM_ROUNDS)
    , breakTimerEnabled(DEFAULT_BREAK_TIMER_ENABLED)
    , currentRound(1)
    , mainTimerStart(0)
    , breakTimerStart(0)
    , mainTimerRemaining(0)
    , breakTimerRemaining(0)
    , breakSirenSounded(false)
    , stateChanged(false)
    , breakEnded(false)
    , roundEnded(false)
{
}

bool Timer::update() {
    // Reset flags
    stateChanged = false;
    breakEnded = false;
    roundEnded = false;

    // Only process if timer is running
    if (state != RUNNING) {
        return false;
    }

    unsigned long now = millis();

    // Calculate elapsed time with overflow protection
    unsigned long mainElapsed = calculateElapsed(mainTimerStart, now);
    unsigned long breakElapsed = calculateElapsed(breakTimerStart, now);

    // Update remaining times
    mainTimerRemaining = (mainElapsed < gameDuration) ? (gameDuration - mainElapsed) : 0;
    breakTimerRemaining = (breakElapsed < breakDuration) ? (breakDuration - breakElapsed) : 0;

    // Check if break period has ended
    if (breakTimerEnabled && !breakSirenSounded && breakTimerRemaining == 0) {
        breakSirenSounded = true;
        breakEnded = true;
        stateChanged = true;
    }

    // Check if round has ended
    if (mainTimerRemaining == 0) {
        roundEnded = true;
        stateChanged = true;

        if (currentRound >= numRounds) {
            // Match finished
            state = FINISHED;
        } else {
            // Start next round
            currentRound++;
            mainTimerStart = millis();
            breakTimerStart = millis();
            breakSirenSounded = false;
        }
    }

    return stateChanged;
}

void Timer::start() {
    if (state == IDLE || state == FINISHED) {
        state = RUNNING;
        currentRound = 1;
        mainTimerStart = millis();
        breakTimerStart = millis();
        mainTimerRemaining = gameDuration;
        breakTimerRemaining = breakDuration;
        breakSirenSounded = false;
    }
}

void Timer::pause() {
    if (state == RUNNING) {
        state = PAUSED;
        // Capture remaining time at pause moment
        unsigned long now = millis();
        unsigned long mainElapsed = calculateElapsed(mainTimerStart, now);
        unsigned long breakElapsed = calculateElapsed(breakTimerStart, now);
        mainTimerRemaining = (mainElapsed < gameDuration) ? (gameDuration - mainElapsed) : 0;
        breakTimerRemaining = (breakElapsed < breakDuration) ? (breakDuration - breakElapsed) : 0;
    }
}

void Timer::resume() {
    if (state == PAUSED) {
        state = RUNNING;
        // Restart timers from remaining time
        unsigned long now = millis();
        mainTimerStart = now - (gameDuration - mainTimerRemaining);
        breakTimerStart = now - (breakDuration - breakTimerRemaining);
    }
}

void Timer::reset() {
    state = IDLE;
    currentRound = 1;
    mainTimerRemaining = 0;
    breakTimerRemaining = 0;
    breakSirenSounded = false;
}

bool Timer::hasBreakEnded() {
    return breakEnded;
}

bool Timer::hasRoundEnded() {
    return roundEnded;
}

bool Timer::isMatchFinished() {
    return state == FINISHED;
}

unsigned long Timer::calculateElapsed(unsigned long start, unsigned long now) {
    // Handle millis() overflow (occurs after ~49 days)
    if (now >= start) {
        return now - start;
    } else {
        // Overflow occurred
        return (0xFFFFFFFF - start) + now + 1;
    }
}
