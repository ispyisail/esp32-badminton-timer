/**
 * Unit tests for Hello Club auto-trigger decision logic
 * Mirrors: src/helloclub.cpp — checkAutoTrigger()
 * and src/main.cpp — the guard that only triggers when timer is IDLE/FINISHED
 */

const TRIGGER_WINDOW_SEC = 120; // 2 minutes

function makeEvent(overrides = {}) {
  return {
    id: 'evt001',
    name: 'Badminton',
    startTime: 1000000,
    endTime: 1007200,
    durationMin: 20,
    numRounds: 3,
    triggered: false,
    ...overrides,
  };
}

/**
 * Replicates checkAutoTrigger(): returns the first untriggered event
 * whose startTime <= now <= startTime + 2min
 */
function checkAutoTrigger(events, nowUtc) {
  for (const evt of events) {
    if (evt.triggered) continue;
    if (nowUtc >= evt.startTime && nowUtc <= evt.startTime + TRIGGER_WINDOW_SEC) {
      return evt;
    }
  }
  return null;
}

/**
 * Replicates the main.cpp guard: auto-trigger only fires
 * when timer is IDLE or FINISHED
 */
function shouldAutoStart(event, timerState) {
  return event !== null && (timerState === 'IDLE' || timerState === 'FINISHED');
}

describe('Auto-Trigger Logic', () => {
  describe('checkAutoTrigger', () => {
    test('triggers at exactly startTime', () => {
      const evt = makeEvent({ startTime: 1000 });
      expect(checkAutoTrigger([evt], 1000)).toBeTruthy();
    });

    test('triggers within 2-minute window', () => {
      const evt = makeEvent({ startTime: 1000 });
      expect(checkAutoTrigger([evt], 1060)).toBeTruthy(); // 1 min in
    });

    test('triggers at edge of 2-minute window', () => {
      const evt = makeEvent({ startTime: 1000 });
      expect(checkAutoTrigger([evt], 1120)).toBeTruthy(); // Exactly 2 min
    });

    test('does NOT trigger after 2-minute window', () => {
      const evt = makeEvent({ startTime: 1000 });
      expect(checkAutoTrigger([evt], 1121)).toBeNull();
    });

    test('does NOT trigger before startTime', () => {
      const evt = makeEvent({ startTime: 1000 });
      expect(checkAutoTrigger([evt], 999)).toBeNull();
    });

    test('skips already-triggered events', () => {
      const evt = makeEvent({ startTime: 1000, triggered: true });
      expect(checkAutoTrigger([evt], 1000)).toBeNull();
    });

    test('picks first untriggered event in window', () => {
      const events = [
        makeEvent({ id: 'a', startTime: 1000, triggered: true }),
        makeEvent({ id: 'b', startTime: 1000, triggered: false }),
        makeEvent({ id: 'c', startTime: 1000, triggered: false }),
      ];
      const result = checkAutoTrigger(events, 1000);
      expect(result.id).toBe('b');
    });

    test('no events — returns null', () => {
      expect(checkAutoTrigger([], 1000)).toBeNull();
    });

    test('multiple events, only one in window', () => {
      const events = [
        makeEvent({ id: 'past', startTime: 500 }),    // Window ended at 620
        makeEvent({ id: 'now', startTime: 1000 }),     // Window 1000-1120
        makeEvent({ id: 'future', startTime: 2000 }),  // Not yet
      ];
      const result = checkAutoTrigger(events, 1050);
      expect(result.id).toBe('now');
    });
  });

  describe('shouldAutoStart (main.cpp guard)', () => {
    const evt = makeEvent();

    test('starts when timer is IDLE', () => {
      expect(shouldAutoStart(evt, 'IDLE')).toBe(true);
    });

    test('starts when timer is FINISHED', () => {
      expect(shouldAutoStart(evt, 'FINISHED')).toBe(true);
    });

    test('does NOT start when timer is RUNNING', () => {
      expect(shouldAutoStart(evt, 'RUNNING')).toBe(false);
    });

    test('does NOT start when timer is PAUSED', () => {
      expect(shouldAutoStart(evt, 'PAUSED')).toBe(false);
    });

    test('does NOT start when no event', () => {
      expect(shouldAutoStart(null, 'IDLE')).toBe(false);
    });
  });

  describe('markTriggered', () => {
    test('marking prevents re-trigger', () => {
      const evt = makeEvent({ startTime: 1000 });
      expect(checkAutoTrigger([evt], 1000)).toBeTruthy();

      evt.triggered = true;
      expect(checkAutoTrigger([evt], 1000)).toBeNull();
    });
  });
});
