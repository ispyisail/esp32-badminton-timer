#pragma once

#include <Arduino.h>

// Timer states
enum TimerState {
    IDLE,
    RUNNING,
    PAUSED,
    FINISHED
};

/**
 * @brief Timer module for managing badminton match timing
 *
 * This class handles all timer-related logic including:
 * - Game and break countdown timers
 * - Round management
 * - State transitions
 * - Time calculations with overflow protection
 */
class Timer {
public:
    /**
     * @brief Constructor
     */
    Timer();

    /**
     * @brief Update timer state (call every loop iteration)
     * @return True if state changed, false otherwise
     */
    bool update();

    /**
     * @brief Start the timer
     */
    void start();

    /**
     * @brief Pause the timer
     */
    void pause();

    /**
     * @brief Resume the timer from paused state
     */
    void resume();

    /**
     * @brief Reset the timer to idle state
     */
    void reset();

    /**
     * @brief Check if break period has ended
     * @return True if break just ended
     */
    bool hasBreakEnded();

    /**
     * @brief Check if round has ended
     * @return True if round just ended
     */
    bool hasRoundEnded();

    /**
     * @brief Check if all rounds are complete
     * @return True if match is finished
     */
    bool isMatchFinished();

    /**
     * @brief Get current timer state
     */
    TimerState getState() const { return state; }

    /**
     * @brief Get main timer remaining time in milliseconds
     */
    unsigned long getMainTimerRemaining() const { return mainTimerRemaining; }

    /**
     * @brief Get break timer remaining time in milliseconds
     */
    unsigned long getBreakTimerRemaining() const { return breakTimerRemaining; }

    /**
     * @brief Get current round number (1-indexed)
     */
    unsigned int getCurrentRound() const { return currentRound; }

    /**
     * @brief Get total number of rounds
     */
    unsigned int getNumRounds() const { return numRounds; }

    /**
     * @brief Get game duration setting
     */
    unsigned long getGameDuration() const { return gameDuration; }

    /**
     * @brief Get break duration setting
     */
    unsigned long getBreakDuration() const { return breakDuration; }

    /**
     * @brief Check if break timer is enabled
     */
    bool isBreakTimerEnabled() const { return breakTimerEnabled; }

    /**
     * @brief Set game duration
     * @param duration Duration in milliseconds
     */
    void setGameDuration(unsigned long duration) { gameDuration = duration; }

    /**
     * @brief Set break duration
     * @param duration Duration in milliseconds
     */
    void setBreakDuration(unsigned long duration) { breakDuration = duration; }

    /**
     * @brief Set number of rounds
     * @param rounds Number of rounds
     */
    void setNumRounds(unsigned int rounds) { numRounds = rounds; }

    /**
     * @brief Set break timer enabled/disabled
     * @param enabled True to enable, false to disable
     */
    void setBreakTimerEnabled(bool enabled) { breakTimerEnabled = enabled; }

private:
    // Timer state
    TimerState state;

    // Timer settings
    unsigned long gameDuration;
    unsigned long breakDuration;
    unsigned int numRounds;
    bool breakTimerEnabled;

    // Current state
    unsigned int currentRound;
    unsigned long mainTimerStart;
    unsigned long breakTimerStart;
    unsigned long mainTimerRemaining;
    unsigned long breakTimerRemaining;
    bool breakSirenSounded;

    // State change flags
    bool stateChanged;
    bool breakEnded;
    bool roundEnded;

    /**
     * @brief Calculate elapsed time with overflow protection
     * @param start Start time from millis()
     * @param now Current time from millis()
     * @return Elapsed time in milliseconds
     */
    unsigned long calculateElapsed(unsigned long start, unsigned long now);
};
