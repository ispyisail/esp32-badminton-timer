/**
 * Unit tests for formatTime() — converts milliseconds to MM:SS display
 * Source: data/script.js:166-172
 */

// Extract the pure function (no DOM dependency)
function formatTime(milliseconds) {
  if (isNaN(milliseconds) || milliseconds < 0) milliseconds = 0;
  const totalSeconds = Math.floor(milliseconds / 1000);
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds % 60;
  return `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
}

describe('formatTime', () => {
  test('zero milliseconds shows 00:00', () => {
    expect(formatTime(0)).toBe('00:00');
  });

  test('negative values clamp to 00:00', () => {
    expect(formatTime(-1000)).toBe('00:00');
    expect(formatTime(-999999)).toBe('00:00');
  });

  test('NaN clamps to 00:00', () => {
    expect(formatTime(NaN)).toBe('00:00');
    expect(formatTime(undefined)).toBe('00:00');
    expect(formatTime(null)).toBe('00:00');
  });

  test('exact seconds', () => {
    expect(formatTime(1000)).toBe('00:01');
    expect(formatTime(10000)).toBe('00:10');
    expect(formatTime(59000)).toBe('00:59');
  });

  test('exact minutes', () => {
    expect(formatTime(60000)).toBe('01:00');
    expect(formatTime(120000)).toBe('02:00');
    expect(formatTime(600000)).toBe('10:00');
  });

  test('minutes and seconds', () => {
    expect(formatTime(90000)).toBe('01:30');
    expect(formatTime(754000)).toBe('12:34');
  });

  test('12 minute default game duration', () => {
    expect(formatTime(12 * 60 * 1000)).toBe('12:00');
  });

  test('sub-second values floor to current second', () => {
    expect(formatTime(999)).toBe('00:00');
    expect(formatTime(1001)).toBe('00:01');
    expect(formatTime(1999)).toBe('00:01');
    expect(formatTime(61500)).toBe('01:01');
  });

  test('large values (over 60 minutes)', () => {
    expect(formatTime(120 * 60 * 1000)).toBe('120:00');
  });
});
