/**
 * Unit tests for the Timer state machine logic
 * Mirrors: src/timer.cpp — the C++ state machine
 *
 * We reimplement the Timer class in JS to test the logic independently.
 * This ensures the algorithm is correct before deploying to hardware.
 */

const IDLE = 'IDLE';
const RUNNING = 'RUNNING';
const PAUSED = 'PAUSED';
const FINISHED = 'FINISHED';

class Timer {
  constructor() {
    this.state = IDLE;
    this.gameDuration = 12 * 60 * 1000;
    this.numRounds = 3;
    this.currentRound = 1;
    this.mainTimerStart = 0;
    this.mainTimerRemaining = 0;
    this.pauseAfterNext = false;
    this.continuousMode = false;
    this._stateChanged = false;
    this._roundEnded = false;
    this._now = 0; // Simulated time
  }

  setNow(ms) { this._now = ms; }
  advanceTime(ms) { this._now += ms; }

  update() {
    this._stateChanged = false;
    this._roundEnded = false;

    if (this.state !== RUNNING) return false;

    const elapsed = this._now - this.mainTimerStart;
    this.mainTimerRemaining = (elapsed < this.gameDuration)
      ? (this.gameDuration - elapsed) : 0;

    if (this.mainTimerRemaining === 0) {
      this._roundEnded = true;
      this._stateChanged = true;

      if (this.currentRound >= this.numRounds && !this.continuousMode) {
        this.state = FINISHED;
      } else if (this.pauseAfterNext) {
        this.currentRound++;
        this.mainTimerRemaining = this.gameDuration;
        this.state = PAUSED;
        this.pauseAfterNext = false;
      } else {
        this.currentRound++;
        this.mainTimerStart = this._now;
      }
    }

    return this._stateChanged;
  }

  start() {
    if (this.state === IDLE || this.state === FINISHED) {
      this.state = RUNNING;
      this.currentRound = 1;
      this.mainTimerStart = this._now;
      this.mainTimerRemaining = this.gameDuration;
      this.pauseAfterNext = false;
    }
  }

  pause() {
    if (this.state === RUNNING) {
      this.state = PAUSED;
      const elapsed = this._now - this.mainTimerStart;
      this.mainTimerRemaining = (elapsed < this.gameDuration)
        ? (this.gameDuration - elapsed) : 0;
    }
  }

  resume() {
    if (this.state === PAUSED) {
      this.state = RUNNING;
      const elapsed = this.gameDuration - this.mainTimerRemaining;
      this.mainTimerStart = this._now - elapsed;
    }
  }

  startMidRound(round, remainingMs) {
    this.state = RUNNING;
    this.currentRound = round;
    this.mainTimerRemaining = remainingMs;
    this.pauseAfterNext = false;
    const elapsed = this.gameDuration - remainingMs;
    this.mainTimerStart = this._now - elapsed;
  }

  reset() {
    this.state = IDLE;
    this.currentRound = 1;
    this.mainTimerRemaining = 0;
    this.pauseAfterNext = false;
    this.continuousMode = false;
  }

  hasRoundEnded() { return this._roundEnded; }
  isMatchFinished() { return this.state === FINISHED; }
}

describe('Timer State Machine', () => {
  let timer;

  beforeEach(() => {
    timer = new Timer();
    timer.setNow(10000); // Start at arbitrary time
  });

  describe('initial state', () => {
    test('starts in IDLE state', () => {
      expect(timer.state).toBe(IDLE);
    });

    test('initial remaining is 0', () => {
      expect(timer.mainTimerRemaining).toBe(0);
    });

    test('update returns false when IDLE', () => {
      expect(timer.update()).toBe(false);
    });
  });

  describe('start', () => {
    test('transitions from IDLE to RUNNING', () => {
      timer.start();
      expect(timer.state).toBe(RUNNING);
      expect(timer.currentRound).toBe(1);
      expect(timer.mainTimerRemaining).toBe(timer.gameDuration);
    });

    test('can start from FINISHED state', () => {
      timer.state = FINISHED;
      timer.start();
      expect(timer.state).toBe(RUNNING);
      expect(timer.currentRound).toBe(1);
    });

    test('ignores start when RUNNING', () => {
      timer.start();
      timer.advanceTime(5000);
      timer.update();
      const remaining = timer.mainTimerRemaining;
      timer.start(); // Should be ignored
      expect(timer.mainTimerRemaining).toBe(remaining);
    });

    test('ignores start when PAUSED', () => {
      timer.start();
      timer.advanceTime(5000);
      timer.pause();
      const remaining = timer.mainTimerRemaining;
      timer.start(); // Should be ignored
      expect(timer.mainTimerRemaining).toBe(remaining);
      expect(timer.state).toBe(PAUSED);
    });

    test('clears pauseAfterNext on start', () => {
      timer.pauseAfterNext = true;
      timer.start();
      expect(timer.pauseAfterNext).toBe(false);
    });
  });

  describe('countdown', () => {
    test('remaining decreases over time', () => {
      timer.start();
      timer.advanceTime(5000);
      timer.update();
      expect(timer.mainTimerRemaining).toBe(timer.gameDuration - 5000);
    });

    test('remaining cannot go below zero', () => {
      timer.start();
      timer.advanceTime(timer.gameDuration + 10000);
      timer.update();
      // After round end, remaining is either 0 (finished) or gameDuration (next round)
      expect(timer.mainTimerRemaining).toBeGreaterThanOrEqual(0);
    });
  });

  describe('pause / resume', () => {
    test('pausing captures remaining time', () => {
      timer.start();
      timer.advanceTime(30000); // 30 seconds
      timer.pause();
      expect(timer.state).toBe(PAUSED);
      expect(timer.mainTimerRemaining).toBe(timer.gameDuration - 30000);
    });

    test('update returns false when PAUSED', () => {
      timer.start();
      timer.advanceTime(5000);
      timer.pause();
      expect(timer.update()).toBe(false);
    });

    test('resume preserves remaining time', () => {
      timer.start();
      timer.advanceTime(30000);
      timer.pause();
      const pausedRemaining = timer.mainTimerRemaining;

      timer.advanceTime(60000); // Wait a minute while paused
      timer.resume();
      timer.update();

      // After resume, remaining should still be ~pausedRemaining
      expect(timer.mainTimerRemaining).toBe(pausedRemaining);
    });

    test('pause only works when RUNNING', () => {
      timer.pause(); // IDLE - should do nothing
      expect(timer.state).toBe(IDLE);
    });

    test('resume only works when PAUSED', () => {
      timer.resume(); // IDLE - should do nothing
      expect(timer.state).toBe(IDLE);
    });

    test('multiple pause/resume cycles preserve time correctly', () => {
      timer.start();

      // Run for 10s
      timer.advanceTime(10000);
      timer.update();
      timer.pause();
      const remaining1 = timer.mainTimerRemaining;

      // Wait 5s while paused
      timer.advanceTime(5000);
      timer.resume();

      // Run for 10s more
      timer.advanceTime(10000);
      timer.update();
      timer.pause();

      expect(timer.mainTimerRemaining).toBe(remaining1 - 10000);
    });
  });

  describe('round transitions', () => {
    test('advances to next round when timer expires', () => {
      timer.gameDuration = 5000; // 5 seconds for fast testing
      timer.numRounds = 3;
      timer.start();

      timer.advanceTime(5000);
      const changed = timer.update();

      expect(changed).toBe(true);
      expect(timer.hasRoundEnded()).toBe(true);
      expect(timer.currentRound).toBe(2);
      expect(timer.state).toBe(RUNNING);
    });

    test('finishes after all rounds', () => {
      timer.gameDuration = 5000;
      timer.numRounds = 2;
      timer.start();

      // Round 1 ends
      timer.advanceTime(5000);
      timer.update();
      expect(timer.currentRound).toBe(2);
      expect(timer.state).toBe(RUNNING);

      // Round 2 ends
      timer.advanceTime(5000);
      timer.update();
      expect(timer.state).toBe(FINISHED);
      expect(timer.isMatchFinished()).toBe(true);
    });

    test('single round game finishes after one round', () => {
      timer.gameDuration = 5000;
      timer.numRounds = 1;
      timer.start();

      timer.advanceTime(5000);
      timer.update();
      expect(timer.state).toBe(FINISHED);
    });
  });

  describe('pauseAfterNext', () => {
    test('pauses between rounds when enabled', () => {
      timer.gameDuration = 5000;
      timer.numRounds = 3;
      timer.start();
      timer.pauseAfterNext = true;

      timer.advanceTime(5000);
      timer.update();

      expect(timer.state).toBe(PAUSED);
      expect(timer.currentRound).toBe(2);
      expect(timer.mainTimerRemaining).toBe(5000); // Full next round loaded
      expect(timer.pauseAfterNext).toBe(false); // One-shot, auto-cleared
    });

    test('can resume after pauseAfterNext', () => {
      timer.gameDuration = 5000;
      timer.numRounds = 3;
      timer.start();
      timer.pauseAfterNext = true;

      timer.advanceTime(5000);
      timer.update();
      expect(timer.state).toBe(PAUSED);

      timer.resume();
      expect(timer.state).toBe(RUNNING);
      expect(timer.currentRound).toBe(2);
    });
  });

  describe('continuous mode', () => {
    test('keeps going beyond numRounds', () => {
      timer.gameDuration = 5000;
      timer.numRounds = 2;
      timer.continuousMode = true;
      timer.start();

      // Round 1
      timer.advanceTime(5000);
      timer.update();
      expect(timer.currentRound).toBe(2);
      expect(timer.state).toBe(RUNNING);

      // Round 2 — would normally finish, but continuous keeps going
      timer.advanceTime(5000);
      timer.update();
      expect(timer.currentRound).toBe(3);
      expect(timer.state).toBe(RUNNING);

      // Round 10
      for (let i = 3; i < 10; i++) {
        timer.advanceTime(5000);
        timer.update();
      }
      expect(timer.currentRound).toBe(10);
      expect(timer.state).toBe(RUNNING);
    });

    test('pauseAfterNext works in continuous mode', () => {
      timer.gameDuration = 5000;
      timer.numRounds = 2;
      timer.continuousMode = true;
      timer.start();
      timer.pauseAfterNext = true;

      timer.advanceTime(5000);
      timer.update();

      expect(timer.state).toBe(PAUSED);
      expect(timer.currentRound).toBe(2);
    });
  });

  describe('reset', () => {
    test('returns to IDLE from RUNNING', () => {
      timer.start();
      timer.advanceTime(5000);
      timer.reset();
      expect(timer.state).toBe(IDLE);
      expect(timer.currentRound).toBe(1);
      expect(timer.mainTimerRemaining).toBe(0);
    });

    test('clears pauseAfterNext and continuousMode', () => {
      timer.pauseAfterNext = true;
      timer.continuousMode = true;
      timer.reset();
      expect(timer.pauseAfterNext).toBe(false);
      expect(timer.continuousMode).toBe(false);
    });

    test('reset from PAUSED', () => {
      timer.start();
      timer.advanceTime(5000);
      timer.pause();
      timer.reset();
      expect(timer.state).toBe(IDLE);
    });

    test('reset from FINISHED', () => {
      timer.gameDuration = 5000;
      timer.numRounds = 1;
      timer.start();
      timer.advanceTime(5000);
      timer.update();
      expect(timer.state).toBe(FINISHED);
      timer.reset();
      expect(timer.state).toBe(IDLE);
    });
  });

  describe('startMidRound (boot recovery)', () => {
    test('starts at specified round with remaining time', () => {
      timer.gameDuration = 20 * 60 * 1000; // 20 min
      timer.startMidRound(3, 5 * 60 * 1000); // Round 3, 5 min remaining
      expect(timer.state).toBe(RUNNING);
      expect(timer.currentRound).toBe(3);
      expect(timer.mainTimerRemaining).toBe(5 * 60 * 1000);
    });

    test('countdown continues correctly after startMidRound', () => {
      timer.gameDuration = 10000;
      timer.numRounds = 5;
      timer.startMidRound(2, 7000); // 7s left in round 2

      timer.advanceTime(3000);
      timer.update();
      expect(timer.mainTimerRemaining).toBe(4000);
      expect(timer.state).toBe(RUNNING);
    });

    test('round ends at correct time after startMidRound', () => {
      timer.gameDuration = 10000;
      timer.numRounds = 5;
      timer.startMidRound(2, 6000);

      timer.advanceTime(6000);
      timer.update();
      expect(timer.hasRoundEnded()).toBe(true);
      expect(timer.currentRound).toBe(3);
    });

    test('match finishes if startMidRound on last round', () => {
      timer.gameDuration = 10000;
      timer.numRounds = 3;
      timer.startMidRound(3, 2000);

      timer.advanceTime(2000);
      timer.update();
      expect(timer.state).toBe(FINISHED);
    });

    test('clears pauseAfterNext', () => {
      timer.pauseAfterNext = true;
      timer.startMidRound(1, 5000);
      expect(timer.pauseAfterNext).toBe(false);
    });

    test('works from any prior state', () => {
      timer.state = FINISHED;
      timer.startMidRound(2, 5000);
      expect(timer.state).toBe(RUNNING);
    });
  });

  describe('settings', () => {
    test('custom game duration', () => {
      timer.gameDuration = 20 * 60 * 1000;
      timer.start();
      expect(timer.mainTimerRemaining).toBe(20 * 60 * 1000);
    });

    test('custom round count', () => {
      timer.gameDuration = 1000;
      timer.numRounds = 5;
      timer.start();

      for (let i = 0; i < 4; i++) {
        timer.advanceTime(1000);
        timer.update();
        expect(timer.state).toBe(RUNNING);
      }
      timer.advanceTime(1000);
      timer.update();
      expect(timer.state).toBe(FINISHED);
    });
  });
});
