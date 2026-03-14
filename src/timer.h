#pragma once

#include <Arduino.h>

// Timer states
enum TimerState {
    IDLE,
    RUNNING,
    PAUSED,
    FINISHED
};

class Timer {
public:
    Timer();

    bool update();
    void start();
    void pause();
    void resume();
    void reset();

    bool hasRoundEnded();
    bool isMatchFinished();

    TimerState getState() const { return state; }
    unsigned long getMainTimerRemaining() const { return mainTimerRemaining; }
    unsigned int getCurrentRound() const { return currentRound; }
    unsigned int getNumRounds() const { return numRounds; }
    unsigned long getGameDuration() const { return gameDuration; }

    void setGameDuration(unsigned long duration) { gameDuration = duration; }
    void setNumRounds(unsigned int rounds) { numRounds = rounds; }

    // Pause After Next: one-shot flag to pause between rounds
    void setPauseAfterNext(bool enabled) { pauseAfterNext = enabled; }
    bool getPauseAfterNext() const { return pauseAfterNext; }

    // Continuous mode: rounds repeat until externally stopped (e.g. event cutoff)
    void setContinuousMode(bool enabled) { continuousMode = enabled; }
    bool getContinuousMode() const { return continuousMode; }

private:
    TimerState state;

    unsigned long gameDuration;
    unsigned int numRounds;

    unsigned int currentRound;
    unsigned long mainTimerStart;
    unsigned long mainTimerRemaining;

    bool pauseAfterNext;
    bool continuousMode;

    // State change flags
    bool stateChanged;
    bool roundEnded;

    unsigned long calculateElapsed(unsigned long start, unsigned long now);
};
