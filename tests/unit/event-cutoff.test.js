/**
 * Unit tests for event window cutoff logic
 * Mirrors: src/main.cpp — event window enforcement block
 *
 * Tests the decision: when should the timer be force-stopped
 * because the booking window has expired?
 */

/**
 * Replicates the event cutoff check from main.cpp loop().
 * Returns true if the timer should be stopped.
 */
function shouldCutoff(activeEventEndTime, timerState, nowUtc) {
  if (activeEventEndTime <= 0) return false;
  if (timerState !== 'RUNNING' && timerState !== 'PAUSED') return false;
  return nowUtc >= activeEventEndTime;
}

describe('Event Cutoff Logic', () => {
  const endTime = 1773725400; // 2026-03-17 05:30 UTC

  describe('basic cutoff', () => {
    test('cuts off at exactly end time when RUNNING', () => {
      expect(shouldCutoff(endTime, 'RUNNING', endTime)).toBe(true);
    });

    test('cuts off after end time when RUNNING', () => {
      expect(shouldCutoff(endTime, 'RUNNING', endTime + 60)).toBe(true);
    });

    test('does NOT cut off before end time', () => {
      expect(shouldCutoff(endTime, 'RUNNING', endTime - 1)).toBe(false);
    });

    test('cuts off when PAUSED at end time', () => {
      expect(shouldCutoff(endTime, 'PAUSED', endTime)).toBe(true);
    });
  });

  describe('timer state guards', () => {
    test('does NOT cut off when IDLE (timer not running)', () => {
      expect(shouldCutoff(endTime, 'IDLE', endTime + 100)).toBe(false);
    });

    test('does NOT cut off when FINISHED (already done)', () => {
      expect(shouldCutoff(endTime, 'FINISHED', endTime + 100)).toBe(false);
    });
  });

  describe('no active event', () => {
    test('no cutoff when activeEventEndTime is 0 (manual timer)', () => {
      expect(shouldCutoff(0, 'RUNNING', 9999999999)).toBe(false);
    });

    test('no cutoff when activeEventEndTime is negative', () => {
      expect(shouldCutoff(-1, 'RUNNING', 9999999999)).toBe(false);
    });
  });

  describe('realistic scenarios', () => {
    test('5:30pm booking, timer still running at 5:30:01', () => {
      // 5:30pm NZDT = 4:30am UTC = 1773721800
      const bookingEnd = 1773725400; // 5:30am UTC = 6:30pm NZDT...
      expect(shouldCutoff(bookingEnd, 'RUNNING', bookingEnd + 1)).toBe(true);
    });

    test('timer paused during break, booking expires', () => {
      expect(shouldCutoff(endTime, 'PAUSED', endTime)).toBe(true);
    });

    test('timer finished naturally before booking ends — no cutoff', () => {
      expect(shouldCutoff(endTime, 'FINISHED', endTime - 600)).toBe(false);
    });

    test('1 second before end — keep going', () => {
      expect(shouldCutoff(endTime, 'RUNNING', endTime - 1)).toBe(false);
    });

    test('well past end time — still cuts off (late check)', () => {
      expect(shouldCutoff(endTime, 'RUNNING', endTime + 3600)).toBe(true);
    });
  });
});
