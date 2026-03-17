/**
 * Unit tests for parseServerTime() — parses "hh:mm:ss am/pm" format
 * Source: data/script.js:355-368
 */

function parseServerTime(timeStr) {
  const match = timeStr.match(/^(\d{1,2}):(\d{2}):(\d{2})\s*(am|pm)$/i);
  if (!match) return null;
  let h = parseInt(match[1]);
  const m = parseInt(match[2]);
  const s = parseInt(match[3]);
  const ampm = match[4].toLowerCase();
  if (ampm === 'am' && h === 12) h = 0;
  if (ampm === 'pm' && h !== 12) h += 12;
  const now = new Date();
  return new Date(now.getFullYear(), now.getMonth(), now.getDate(), h, m, s);
}

describe('parseServerTime', () => {
  test('parses morning time', () => {
    const result = parseServerTime('09:30:15 AM');
    expect(result).not.toBeNull();
    expect(result.getHours()).toBe(9);
    expect(result.getMinutes()).toBe(30);
    expect(result.getSeconds()).toBe(15);
  });

  test('parses afternoon time', () => {
    const result = parseServerTime('02:45:30 PM');
    expect(result).not.toBeNull();
    expect(result.getHours()).toBe(14);
    expect(result.getMinutes()).toBe(45);
    expect(result.getSeconds()).toBe(30);
  });

  test('parses 12:00:00 PM as noon (12)', () => {
    const result = parseServerTime('12:00:00 PM');
    expect(result).not.toBeNull();
    expect(result.getHours()).toBe(12);
  });

  test('parses 12:00:00 AM as midnight (0)', () => {
    const result = parseServerTime('12:00:00 AM');
    expect(result).not.toBeNull();
    expect(result.getHours()).toBe(0);
  });

  test('handles lowercase am/pm', () => {
    const result = parseServerTime('03:15:00 pm');
    expect(result).not.toBeNull();
    expect(result.getHours()).toBe(15);
  });

  test('returns null for invalid format', () => {
    expect(parseServerTime('invalid')).toBeNull();
    expect(parseServerTime('')).toBeNull();
    expect(parseServerTime('9:30 AM')).toBeNull(); // Missing seconds
    expect(parseServerTime('10:00 AM')).toBeNull(); // Missing seconds
    expect(parseServerTime('not a time 10:00:00 AM')).toBeNull(); // Extra prefix
  });

  test('accepts regex-valid but semantically odd hours (no validation)', () => {
    // The function only uses regex matching, not semantic hour validation
    const result = parseServerTime('25:00:00 AM');
    expect(result).not.toBeNull(); // Regex accepts \d{1,2}
  });

  test('result has today\'s date', () => {
    const result = parseServerTime('10:00:00 AM');
    const now = new Date();
    expect(result.getFullYear()).toBe(now.getFullYear());
    expect(result.getMonth()).toBe(now.getMonth());
    expect(result.getDate()).toBe(now.getDate());
  });
});
