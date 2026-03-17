/**
 * Unit tests for purgeExpired logic
 * Mirrors: src/helloclub.cpp — purgeExpired()
 *
 * Removes events whose endTime has passed.
 */

function makeEvent(id, endTime) {
  return {
    id,
    name: `Event ${id}`,
    startTime: endTime - 7200,
    endTime,
    durationMin: 20,
    numRounds: 3,
    triggered: false,
  };
}

function purgeExpired(events, nowUtc) {
  return events.filter(evt => evt.endTime >= nowUtc);
}

describe('purgeExpired', () => {
  test('removes event that ended before now', () => {
    const events = [makeEvent('a', 1000)];
    expect(purgeExpired(events, 1001)).toHaveLength(0);
  });

  test('keeps event that ends exactly now', () => {
    const events = [makeEvent('a', 1000)];
    expect(purgeExpired(events, 1000)).toHaveLength(1);
  });

  test('keeps event that ends in the future', () => {
    const events = [makeEvent('a', 2000)];
    expect(purgeExpired(events, 1000)).toHaveLength(1);
  });

  test('mixed — keeps future, removes past', () => {
    const events = [
      makeEvent('past1', 500),
      makeEvent('future1', 2000),
      makeEvent('past2', 800),
      makeEvent('future2', 3000),
    ];
    const result = purgeExpired(events, 1000);
    expect(result).toHaveLength(2);
    expect(result.map(e => e.id)).toEqual(['future1', 'future2']);
  });

  test('empty list — no crash', () => {
    expect(purgeExpired([], 1000)).toHaveLength(0);
  });

  test('all expired', () => {
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
});
