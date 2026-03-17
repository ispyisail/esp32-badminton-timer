/**
 * Unit tests for sirenAllowed() logic
 * Mirrors: src/main.cpp — sirenAllowed()
 *
 * The siren should be suppressed after an event's booking window ends,
 * preventing noise outside the allocated time slot.
 */

function sirenAllowed(activeEventEndTime, nowUtc) {
  if (activeEventEndTime === 0) return true;
  return nowUtc < activeEventEndTime;
}

describe('sirenAllowed', () => {
  test('always allowed when no active event (endTime=0)', () => {
    expect(sirenAllowed(0, 1000000)).toBe(true);
  });

  test('allowed during event window', () => {
    const endTime = 1773725400; // 2026-03-17 05:30 UTC
    const now = 1773720000;     // 1.5 hours before end
    expect(sirenAllowed(endTime, now)).toBe(true);
  });

  test('NOT allowed at exactly end time', () => {
    const endTime = 1773725400;
    expect(sirenAllowed(endTime, endTime)).toBe(false);
  });

  test('NOT allowed after end time', () => {
    const endTime = 1773725400;
    expect(sirenAllowed(endTime, endTime + 1)).toBe(false);
  });

  test('allowed 1 second before end', () => {
    const endTime = 1773725400;
    expect(sirenAllowed(endTime, endTime - 1)).toBe(true);
  });

  test('allowed well before end time', () => {
    const endTime = 1773725400;
    expect(sirenAllowed(endTime, endTime - 7200)).toBe(true);
  });

  test('NOT allowed long after end time', () => {
    const endTime = 1773725400;
    expect(sirenAllowed(endTime, endTime + 86400)).toBe(false);
  });

  test('manual timer (no event) — siren always allowed', () => {
    // When timer is started manually without HC event, endTime stays 0
    expect(sirenAllowed(0, 0)).toBe(true);
    expect(sirenAllowed(0, 9999999999)).toBe(true);
  });
});
