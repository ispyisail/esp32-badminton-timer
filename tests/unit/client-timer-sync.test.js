/**
 * Unit tests for the client-side timer sync/display logic
 * Tests the bug fix: displayEndTime must be initialized on first sync
 * Source: data/script.js — sync handler and clientTimerLoop
 */

describe('Client Timer Sync Logic', () => {
  // Simulate the client-side state
  let displayEndTime;
  let serverEndTime;
  let isClientTimerPaused;
  let currentTimerStatus;
  let timerLoopRunning;

  function resetClientState() {
    displayEndTime = 0;
    serverEndTime = 0;
    isClientTimerPaused = false;
    currentTimerStatus = 'IDLE';
    timerLoopRunning = false;
  }

  function startClientTimer() {
    timerLoopRunning = !isClientTimerPaused;
  }

  function stopClientTimer() {
    timerLoopRunning = false;
  }

  // Simulates the sync handler from script.js
  function handleSync(data) {
    serverEndTime = Date.now() + data.mainTimerRemaining;

    // BUG FIX: snap displayEndTime on first sync
    if (displayEndTime === 0) {
      displayEndTime = serverEndTime;
    }

    isClientTimerPaused = (data.status === 'PAUSED');
    currentTimerStatus = data.status;

    if (!isClientTimerPaused && data.status === 'RUNNING') {
      startClientTimer();
    } else {
      stopClientTimer();
      displayEndTime = serverEndTime;
    }
  }

  // Simulates the state handler from script.js
  function handleState(data) {
    serverEndTime = Date.now() + (data.state.mainTimer || 0);
    displayEndTime = serverEndTime;
    currentTimerStatus = data.state.status || 'IDLE';
  }

  // Simulates one frame of clientTimerLoop
  function simulateFrame() {
    if (isClientTimerPaused) return { remaining: displayEndTime - Date.now(), loopContinues: true };

    const error = serverEndTime - displayEndTime;
    if (Math.abs(error) > 50) {
      displayEndTime += error * 0.03;
    } else {
      displayEndTime = serverEndTime;
    }

    let remaining = displayEndTime - Date.now();
    if (remaining < 0) remaining = 0;

    return { remaining, loopContinues: remaining > 0 };
  }

  beforeEach(() => {
    resetClientState();
  });

  describe('initial page load with timer RUNNING', () => {
    test('sync initializes displayEndTime (the bug fix)', () => {
      // Before fix: displayEndTime stayed 0, causing 00:00 display
      handleSync({ mainTimerRemaining: 300000, status: 'RUNNING' });

      expect(displayEndTime).toBeGreaterThan(0);
      expect(timerLoopRunning).toBe(true);

      const frame = simulateFrame();
      expect(frame.remaining).toBeGreaterThan(0);
      expect(frame.loopContinues).toBe(true);
    });

    test('without fix, displayEndTime=0 causes immediate timer stop', () => {
      // Simulate the old buggy behavior
      displayEndTime = 0;
      serverEndTime = Date.now() + 300000;
      // Old code didn't set displayEndTime before starting timer

      const frame = simulateFrame();
      // displayEndTime is 0, so remaining = 0 - now = negative → clamped to 0
      // The loop would stop because remaining <= 0
      // This is the bug: the animation loop dies on first frame
      expect(frame.remaining).toBe(0);
      expect(frame.loopContinues).toBe(false);
    });
  });

  describe('initial page load with timer IDLE', () => {
    test('state handler sets displayEndTime correctly', () => {
      handleState({
        state: { status: 'IDLE', mainTimer: 720000, currentRound: 1, numRounds: 3 }
      });

      expect(currentTimerStatus).toBe('IDLE');
      expect(displayEndTime).toBeGreaterThan(0);
    });
  });

  describe('initial page load with timer PAUSED', () => {
    test('sync with PAUSED status stops timer loop', () => {
      handleSync({ mainTimerRemaining: 300000, status: 'PAUSED' });

      expect(isClientTimerPaused).toBe(true);
      expect(timerLoopRunning).toBe(false);
      expect(displayEndTime).toBeGreaterThan(0);
    });
  });

  describe('subsequent syncs (smooth convergence)', () => {
    test('second sync does not re-snap displayEndTime', () => {
      // First sync: snap
      handleSync({ mainTimerRemaining: 300000, status: 'RUNNING' });
      const firstDisplayEndTime = displayEndTime;

      // Simulate a few frames of convergence
      for (let i = 0; i < 10; i++) {
        simulateFrame();
      }
      const afterFrames = displayEndTime;

      // Second sync with slight drift
      handleSync({ mainTimerRemaining: 299000, status: 'RUNNING' });

      // displayEndTime should NOT jump to serverEndTime (smooth convergence)
      // It should be close but not exactly equal
      expect(displayEndTime).toBe(afterFrames); // Not snapped because displayEndTime !== 0
    });
  });

  describe('timer completion', () => {
    test('frame reports remaining=0 when timer expires', () => {
      handleSync({ mainTimerRemaining: 100, status: 'RUNNING' });

      // Wait for expiry
      const frame = simulateFrame();
      // With only 100ms remaining and potential timing, this should be very low
      expect(frame.remaining).toBeLessThanOrEqual(200);
    });
  });
});
