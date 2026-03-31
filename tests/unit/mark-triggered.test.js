/**
 * Unit tests for markTriggered logic
 * Mirrors: src/helloclub.cpp — markTriggered(id, startTime)
 *
 * markTriggered must match on BOTH id AND startTime to correctly
 * handle recurring events (which share the same id but have different startTimes).
 */

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
 * Replicates markTriggered(id, startTime) from helloclub.cpp
 * Finds the event matching both id AND startTime, sets triggered=true
 */
function markTriggered(events, id, startTime) {
  for (const evt of events) {
    if (evt.id === id && evt.startTime === startTime) {
      evt.triggered = true;
      return true;
    }
  }
  return false;
}

describe('markTriggered', () => {
  describe('basic matching', () => {
    test('marks event when id and startTime both match', () => {
      const events = [makeEvent({ id: 'a', startTime: 1000 })];
      const found = markTriggered(events, 'a', 1000);
      expect(found).toBe(true);
      expect(events[0].triggered).toBe(true);
    });

    test('does NOT mark when only id matches', () => {
      const events = [makeEvent({ id: 'a', startTime: 1000 })];
      const found = markTriggered(events, 'a', 2000);
      expect(found).toBe(false);
      expect(events[0].triggered).toBe(false);
    });

    test('does NOT mark when only startTime matches', () => {
      const events = [makeEvent({ id: 'a', startTime: 1000 })];
      const found = markTriggered(events, 'b', 1000);
      expect(found).toBe(false);
      expect(events[0].triggered).toBe(false);
    });

    test('returns false when no events', () => {
      expect(markTriggered([], 'a', 1000)).toBe(false);
    });
  });

  describe('recurring events', () => {
    test('marks only the correct occurrence', () => {
      const events = [
        makeEvent({ id: 'weekly', startTime: 1000 }),
        makeEvent({ id: 'weekly', startTime: 2000 }),
        makeEvent({ id: 'weekly', startTime: 3000 }),
      ];

      markTriggered(events, 'weekly', 2000);
      expect(events[0].triggered).toBe(false);
      expect(events[1].triggered).toBe(true);
      expect(events[2].triggered).toBe(false);
    });

    test('can mark multiple occurrences independently', () => {
      const events = [
        makeEvent({ id: 'weekly', startTime: 1000 }),
        makeEvent({ id: 'weekly', startTime: 2000 }),
      ];

      markTriggered(events, 'weekly', 1000);
      markTriggered(events, 'weekly', 2000);
      expect(events[0].triggered).toBe(true);
      expect(events[1].triggered).toBe(true);
    });

    test('marking is idempotent', () => {
      const events = [makeEvent({ id: 'a', startTime: 1000, triggered: true })];
      markTriggered(events, 'a', 1000);
      expect(events[0].triggered).toBe(true);
    });
  });

  describe('mixed events', () => {
    test('marks correct event among different IDs', () => {
      const events = [
        makeEvent({ id: 'a', startTime: 1000 }),
        makeEvent({ id: 'b', startTime: 1000 }),
        makeEvent({ id: 'c', startTime: 2000 }),
      ];

      markTriggered(events, 'b', 1000);
      expect(events[0].triggered).toBe(false);
      expect(events[1].triggered).toBe(true);
      expect(events[2].triggered).toBe(false);
    });
  });
});
