/**
 * Unit tests for boot recovery cancellation logic
 * Mirrors: src/main.cpp — cancel flag in reset handler + boot recovery gate
 *
 * Tests the decision logic: given a recovery candidate and a cancel flag,
 * should we recover or skip?
 */

/**
 * Simulates the boot recovery decision from main.cpp loop():
 *   - recovery: { shouldRecover, eventId, ... } from checkMidEventRecovery()
 *   - cancelledId: string from NVS "evt_cancel" key (or "" if none)
 *   Returns true if timer should be resumed.
 */
function shouldResumeTimer(recovery, cancelledId) {
  if (!recovery.shouldRecover) return false;
  if (recovery.eventId === cancelledId) return false;
  return true;
}

/**
 * Simulates the reset handler's cancel flag logic:
 *   - activeEventId: current event ID (or "" if no active event)
 *   Returns the value to write to NVS, or null if no write needed.
 */
function getCancelFlagOnReset(activeEventId) {
  if (activeEventId && activeEventId.length > 0) {
    return activeEventId;
  }
  return null;
}

describe('Boot Recovery Cancel Logic', () => {
  describe('shouldResumeTimer', () => {
    test('resumes when recovery valid and no cancel flag', () => {
      expect(shouldResumeTimer(
        { shouldRecover: true, eventId: 'abc123' },
        ''
      )).toBe(true);
    });

    test('skips when recovery event matches cancel flag', () => {
      expect(shouldResumeTimer(
        { shouldRecover: true, eventId: 'abc123' },
        'abc123'
      )).toBe(true === false);
    });

    test('resumes when cancel flag is for different event', () => {
      expect(shouldResumeTimer(
        { shouldRecover: true, eventId: 'abc123' },
        'xyz789'
      )).toBe(true);
    });

    test('skips when no recovery needed', () => {
      expect(shouldResumeTimer(
        { shouldRecover: false, eventId: '' },
        ''
      )).toBe(false);
    });

    test('skips when no recovery even with stale cancel', () => {
      expect(shouldResumeTimer(
        { shouldRecover: false, eventId: '' },
        'old_event'
      )).toBe(false);
    });
  });

  describe('getCancelFlagOnReset', () => {
    test('returns event ID when active event exists', () => {
      expect(getCancelFlagOnReset('abc123')).toBe('abc123');
    });

    test('returns null when no active event', () => {
      expect(getCancelFlagOnReset('')).toBeNull();
    });

    test('returns null for undefined/empty', () => {
      expect(getCancelFlagOnReset('')).toBeNull();
    });
  });

  describe('full scenarios', () => {
    test('scenario: normal boot during event — resumes', () => {
      const cancelledId = '';
      const recovery = { shouldRecover: true, eventId: 'evt_001', currentRound: 2, remainingMs: 300000 };
      expect(shouldResumeTimer(recovery, cancelledId)).toBe(true);
    });

    test('scenario: operator reset then reboot — skips same event', () => {
      // Operator resets during event evt_001
      const cancelFlag = getCancelFlagOnReset('evt_001');
      expect(cancelFlag).toBe('evt_001');

      // ESP reboots, finds evt_001 still in window
      const recovery = { shouldRecover: true, eventId: 'evt_001', currentRound: 1, remainingMs: 600000 };
      expect(shouldResumeTimer(recovery, cancelFlag)).toBe(false);
    });

    test('scenario: operator reset then NEW event starts — resumes new event', () => {
      // Operator resets during event evt_001
      const cancelFlag = getCancelFlagOnReset('evt_001');

      // ESP reboots during a DIFFERENT event
      const recovery = { shouldRecover: true, eventId: 'evt_002', currentRound: 1, remainingMs: 1200000 };
      expect(shouldResumeTimer(recovery, cancelFlag)).toBe(true);
    });

    test('scenario: auto-trigger clears stale cancel on new event start', () => {
      // After auto-trigger, cancel flag is cleared (empty string)
      // Next reboot during that event should recover
      const cancelledId = '';
      const recovery = { shouldRecover: true, eventId: 'evt_003', currentRound: 3, remainingMs: 60000 };
      expect(shouldResumeTimer(recovery, cancelledId)).toBe(true);
    });

    test('scenario: reset with no active event — no cancel flag written', () => {
      // Manual timer (no HC event), operator resets
      const cancelFlag = getCancelFlagOnReset('');
      expect(cancelFlag).toBeNull();
    });

    test('scenario: all rounds completed — no recovery', () => {
      const recovery = { shouldRecover: false, eventId: '' };
      expect(shouldResumeTimer(recovery, '')).toBe(false);
    });
  });

  describe('composite key (id:startTime) for recurring events', () => {
    /**
     * The cancel/recovery key for recurring events must include startTime
     * to avoid one occurrence's cancel blocking a different occurrence.
     */
    function makeCompositeKey(eventId, startTime) {
      return `${eventId}:${startTime}`;
    }

    function shouldResumeWithCompositeKey(recovery, cancelledKey) {
      if (!recovery.shouldRecover) return false;
      const recoveryKey = makeCompositeKey(recovery.eventId, recovery.startTime);
      if (recoveryKey === cancelledKey) return false;
      return true;
    }

    test('composite key blocks same occurrence', () => {
      const cancelKey = makeCompositeKey('weekly', 1000);
      const recovery = { shouldRecover: true, eventId: 'weekly', startTime: 1000 };
      expect(shouldResumeWithCompositeKey(recovery, cancelKey)).toBe(false);
    });

    test('composite key allows different occurrence of same event', () => {
      // Cancelled last week's occurrence
      const cancelKey = makeCompositeKey('weekly', 1000);
      // This week's occurrence has different startTime
      const recovery = { shouldRecover: true, eventId: 'weekly', startTime: 604800 + 1000 };
      expect(shouldResumeWithCompositeKey(recovery, cancelKey)).toBe(true);
    });

    test('composite key allows different event entirely', () => {
      const cancelKey = makeCompositeKey('evt_a', 1000);
      const recovery = { shouldRecover: true, eventId: 'evt_b', startTime: 1000 };
      expect(shouldResumeWithCompositeKey(recovery, cancelKey)).toBe(true);
    });

    test('no cancel key — always resumes', () => {
      const recovery = { shouldRecover: true, eventId: 'weekly', startTime: 1000 };
      expect(shouldResumeWithCompositeKey(recovery, '')).toBe(true);
    });

    test('scenario: cancel weekly Tuesday, reboot during weekly Thursday', () => {
      const tuesdayStart = 1773100800;  // Tuesday
      const thursdayStart = 1773273600;  // Thursday

      const cancelKey = makeCompositeKey('badminton', tuesdayStart);
      const recovery = {
        shouldRecover: true,
        eventId: 'badminton',
        startTime: thursdayStart,
      };
      expect(shouldResumeWithCompositeKey(recovery, cancelKey)).toBe(true);
    });
  });
});
