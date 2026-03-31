/**
 * Unit tests for staged events pattern (race condition fix)
 * Mirrors: src/helloclub.cpp — applyStagedEvents()
 *
 * The background fetch writes to a staging area. The main loop
 * applies staged events, preserving triggered flags by matching
 * on BOTH id AND startTime (not just id — recurring events share IDs).
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
 * Replicates applyStagedEvents() from helloclub.cpp:
 * Merges staged events into the live cache, preserving triggered state
 * from existing events that match on BOTH id AND startTime.
 */
function applyStagedEvents(existingEvents, stagedEvents) {
  for (const staged of stagedEvents) {
    for (const existing of existingEvents) {
      if (existing.id === staged.id && existing.startTime === staged.startTime) {
        staged.triggered = existing.triggered;
        break;
      }
    }
  }
  return [...stagedEvents];
}

describe('Staged Events — applyStagedEvents()', () => {
  describe('triggered flag preservation', () => {
    test('preserves triggered=true for matching event (same id + startTime)', () => {
      const existing = [makeEvent({ id: 'a', startTime: 1000, triggered: true })];
      const staged = [makeEvent({ id: 'a', startTime: 1000, triggered: false })];

      const result = applyStagedEvents(existing, staged);
      expect(result[0].triggered).toBe(true);
    });

    test('does NOT preserve triggered if only id matches (different startTime)', () => {
      const existing = [makeEvent({ id: 'a', startTime: 1000, triggered: true })];
      const staged = [makeEvent({ id: 'a', startTime: 2000, triggered: false })];

      const result = applyStagedEvents(existing, staged);
      expect(result[0].triggered).toBe(false);
    });

    test('does NOT preserve triggered if only startTime matches (different id)', () => {
      const existing = [makeEvent({ id: 'a', startTime: 1000, triggered: true })];
      const staged = [makeEvent({ id: 'b', startTime: 1000, triggered: false })];

      const result = applyStagedEvents(existing, staged);
      expect(result[0].triggered).toBe(false);
    });

    test('handles no existing events (fresh cache)', () => {
      const staged = [makeEvent({ id: 'a', startTime: 1000, triggered: false })];
      const result = applyStagedEvents([], staged);
      expect(result[0].triggered).toBe(false);
    });

    test('handles no staged events', () => {
      const result = applyStagedEvents([makeEvent()], []);
      expect(result).toHaveLength(0);
    });
  });

  describe('recurring events (same id, different startTimes)', () => {
    test('recurring event: only matching occurrence keeps triggered', () => {
      const existing = [
        makeEvent({ id: 'recurring', startTime: 1000, triggered: true }),
        makeEvent({ id: 'recurring', startTime: 2000, triggered: false }),
      ];
      const staged = [
        makeEvent({ id: 'recurring', startTime: 1000, triggered: false }),
        makeEvent({ id: 'recurring', startTime: 2000, triggered: false }),
        makeEvent({ id: 'recurring', startTime: 3000, triggered: false }),
      ];

      const result = applyStagedEvents(existing, staged);
      expect(result[0].triggered).toBe(true);  // 1000 was triggered
      expect(result[1].triggered).toBe(false);  // 2000 was not
      expect(result[2].triggered).toBe(false);  // 3000 is new
    });

    test('recurring event: both occurrences triggered independently', () => {
      const existing = [
        makeEvent({ id: 'recurring', startTime: 1000, triggered: true }),
        makeEvent({ id: 'recurring', startTime: 2000, triggered: true }),
      ];
      const staged = [
        makeEvent({ id: 'recurring', startTime: 1000, triggered: false }),
        makeEvent({ id: 'recurring', startTime: 2000, triggered: false }),
      ];

      const result = applyStagedEvents(existing, staged);
      expect(result[0].triggered).toBe(true);
      expect(result[1].triggered).toBe(true);
    });

    test('recurring event: API returns new occurrence, old one purged', () => {
      // Existing cache has old occurrence that was triggered
      const existing = [
        makeEvent({ id: 'recurring', startTime: 1000, triggered: true }),
      ];
      // New fetch returns a different occurrence (next week)
      const staged = [
        makeEvent({ id: 'recurring', startTime: 604800 + 1000, triggered: false }),
      ];

      const result = applyStagedEvents(existing, staged);
      expect(result[0].triggered).toBe(false); // Different startTime = no match
    });
  });

  describe('mixed events', () => {
    test('preserves triggered flags for multiple matching events', () => {
      const existing = [
        makeEvent({ id: 'a', startTime: 1000, triggered: true }),
        makeEvent({ id: 'b', startTime: 2000, triggered: false }),
        makeEvent({ id: 'c', startTime: 3000, triggered: true }),
      ];
      const staged = [
        makeEvent({ id: 'a', startTime: 1000 }),
        makeEvent({ id: 'b', startTime: 2000 }),
        makeEvent({ id: 'c', startTime: 3000 }),
        makeEvent({ id: 'd', startTime: 4000 }),
      ];

      const result = applyStagedEvents(existing, staged);
      expect(result[0].triggered).toBe(true);
      expect(result[1].triggered).toBe(false);
      expect(result[2].triggered).toBe(true);
      expect(result[3].triggered).toBe(false);
    });

    test('staged replaces existing — events not in staged are dropped', () => {
      const existing = [
        makeEvent({ id: 'old', startTime: 500, triggered: true }),
        makeEvent({ id: 'keep', startTime: 1000, triggered: true }),
      ];
      const staged = [
        makeEvent({ id: 'keep', startTime: 1000 }),
        makeEvent({ id: 'new', startTime: 2000 }),
      ];

      const result = applyStagedEvents(existing, staged);
      expect(result).toHaveLength(2);
      expect(result[0].id).toBe('keep');
      expect(result[0].triggered).toBe(true);
      expect(result[1].id).toBe('new');
      expect(result[1].triggered).toBe(false);
    });
  });
});
