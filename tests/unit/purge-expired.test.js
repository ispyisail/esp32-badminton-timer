/**
 * Unit tests for purgeExpired logic
 * Mirrors: src/helloclub.cpp — purgeExpired()
 *
 * Removes events whose effective end time + grace period has passed.
 * - Untriggered events: endTime + 5 min grace period
 * - Triggered events: max(endTime, startTime + play duration) + 5 min grace period
 *   This handles short bookings where endTime << actual play time.
 */

const PURGE_GRACE_SEC = 300; // 5 minutes

function makeEvent(id, endTime, overrides = {}) {
  return {
    id,
    name: `Event ${id}`,
    startTime: endTime - 7200,
    endTime,
    durationMin: 20,
    numRounds: 3,
    triggered: false,
    ...overrides,
  };
}

/**
 * Updated purgeExpired matching current firmware:
 * - Triggered events use max(endTime, startTime + play duration)
 * - 5-minute grace period for all events
 */
function purgeExpired(events, nowUtc) {
  return events.filter(evt => {
    if (evt.triggered) {
      // Calculate actual play end time
      let playEnd = evt.startTime;
      if (evt.numRounds > 0) {
        playEnd += evt.numRounds * evt.durationMin * 60;
      } else {
        playEnd += evt.durationMin * 60;
      }
      const effectiveEnd = Math.max(evt.endTime, playEnd);
      return effectiveEnd + PURGE_GRACE_SEC >= nowUtc;
    }
    // Not triggered — keep for grace period after endTime
    return evt.endTime + PURGE_GRACE_SEC >= nowUtc;
  });
}

describe('purgeExpired', () => {
  test('removes event well past end + grace period', () => {
    const events = [makeEvent('a', 1000)];
    expect(purgeExpired(events, 1000 + PURGE_GRACE_SEC + 1)).toHaveLength(0);
  });

  test('keeps event within grace period after endTime', () => {
    const events = [makeEvent('a', 1000)];
    // Exactly at grace boundary — should keep
    expect(purgeExpired(events, 1000 + PURGE_GRACE_SEC)).toHaveLength(1);
  });

  test('keeps event that ends exactly now', () => {
    const events = [makeEvent('a', 1000)];
    expect(purgeExpired(events, 1000)).toHaveLength(1);
  });

  test('keeps event that ends in the future', () => {
    const events = [makeEvent('a', 2000)];
    expect(purgeExpired(events, 1000)).toHaveLength(1);
  });

  test('mixed — keeps future and grace-period, removes old', () => {
    const events = [
      makeEvent('old', 500),       // 500 + 300 = 800 < 1000
      makeEvent('grace', 800),     // 800 + 300 = 1100 > 1000
      makeEvent('future', 2000),   // way in the future
    ];
    const result = purgeExpired(events, 1000);
    expect(result).toHaveLength(2);
    expect(result.map(e => e.id)).toEqual(['grace', 'future']);
  });

  test('empty list — no crash', () => {
    expect(purgeExpired([], 1000)).toHaveLength(0);
  });

  test('all expired past grace period', () => {
    const events = [makeEvent('a', 100), makeEvent('b', 200), makeEvent('c', 300)];
    expect(purgeExpired(events, 1000)).toHaveLength(0);
  });

  test('none expired', () => {
    const events = [makeEvent('a', 2000), makeEvent('b', 3000)];
    expect(purgeExpired(events, 1000)).toHaveLength(2);
  });

  test('preserves event properties', () => {
    const events = [makeEvent('keep', 2000)];
    const result = purgeExpired(events, 1000);
    expect(result[0].id).toBe('keep');
    expect(result[0].name).toBe('Event keep');
    expect(result[0].durationMin).toBe(20);
  });

  describe('grace period for untriggered events', () => {
    test('untriggered event kept 4 minutes after endTime', () => {
      const events = [makeEvent('a', 1000)];
      expect(purgeExpired(events, 1000 + 240)).toHaveLength(1);
    });

    test('untriggered event purged 6 minutes after endTime', () => {
      const events = [makeEvent('a', 1000)];
      expect(purgeExpired(events, 1000 + 360)).toHaveLength(0);
    });

    test('short booking: endTime 1 min after start, kept within grace', () => {
      // Short booking: start=1000, end=1060 (1 minute booking)
      // Without grace, this gets purged almost immediately
      const events = [makeEvent('short', 1060, { startTime: 1000 })];
      expect(purgeExpired(events, 1100)).toHaveLength(1); // within grace
      expect(purgeExpired(events, 1060 + PURGE_GRACE_SEC + 1)).toHaveLength(0); // past grace
    });
  });

  describe('triggered events use play duration', () => {
    test('triggered event with short booking uses play duration for purge', () => {
      // Short booking: start=1000, endTime=1060 (1 min booking)
      // Timer: 3 rounds x 20 min = 3600 sec play time
      // Play end: 1000 + 3600 = 4600
      const events = [makeEvent('short', 1060, {
        startTime: 1000,
        triggered: true,
        durationMin: 20,
        numRounds: 3,
      })];

      // At 2000: play end (4600) + grace (300) = 4900 > 2000 → kept
      expect(purgeExpired(events, 2000)).toHaveLength(1);

      // At 4800: play end (4600) + grace (300) = 4900 > 4800 → still kept
      expect(purgeExpired(events, 4800)).toHaveLength(1);

      // At 5000: play end (4600) + grace (300) = 4900 < 5000 → purged
      expect(purgeExpired(events, 5000)).toHaveLength(0);
    });

    test('triggered event with long booking uses booking endTime', () => {
      // Long booking: start=1000, endTime=8200 (2 hours)
      // Timer: 3 rounds x 20 min = 3600 sec
      // endTime (8200) > play end (4600), so endTime wins
      const events = [makeEvent('long', 8200, {
        startTime: 1000,
        triggered: true,
        durationMin: 20,
        numRounds: 3,
      })];

      // At 5000: endTime (8200) + grace (300) = 8500 > 5000 → kept
      expect(purgeExpired(events, 5000)).toHaveLength(1);

      // At 8600: endTime (8200) + grace (300) = 8500 < 8600 → purged
      expect(purgeExpired(events, 8600)).toHaveLength(0);
    });

    test('continuous mode triggered: uses single round as minimum play', () => {
      // Continuous mode: numRounds=0, durationMin=20
      // Play end: startTime + 20min = 1000 + 1200 = 2200
      // endTime (8200) > play end (2200), so endTime wins
      const events = [makeEvent('continuous', 8200, {
        startTime: 1000,
        triggered: true,
        durationMin: 20,
        numRounds: 0,
      })];

      expect(purgeExpired(events, 8400)).toHaveLength(1); // within grace of endTime
      expect(purgeExpired(events, 8600)).toHaveLength(0); // past grace
    });

    test('continuous mode with short booking uses round duration', () => {
      // Continuous, short booking: endTime=1060 (1 min)
      // Play end: 1000 + 1200 = 2200
      const events = [makeEvent('cont-short', 1060, {
        startTime: 1000,
        triggered: true,
        durationMin: 20,
        numRounds: 0,
      })];

      expect(purgeExpired(events, 1200)).toHaveLength(1); // play still going
      expect(purgeExpired(events, 2600)).toHaveLength(0); // 2200 + 300 = 2500 < 2600
    });

    test('untriggered event does NOT use play duration', () => {
      // Same short booking but NOT triggered — uses raw endTime
      const events = [makeEvent('not-triggered', 1060, {
        startTime: 1000,
        triggered: false,
        durationMin: 20,
        numRounds: 3,
      })];

      // endTime (1060) + grace (300) = 1360
      expect(purgeExpired(events, 1300)).toHaveLength(1); // within grace
      expect(purgeExpired(events, 1400)).toHaveLength(0); // past grace
    });
  });
});
