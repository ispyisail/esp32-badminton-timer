/**
 * Unit tests for formatClock() — formats Date to "hh:mm:ss am/pm"
 * Source: data/script.js:370-377
 */

function formatClock(date) {
  let h = date.getHours();
  const m = String(date.getMinutes()).padStart(2, '0');
  const s = String(date.getSeconds()).padStart(2, '0');
  const ampm = h >= 12 ? 'pm' : 'am';
  h = h % 12 || 12;
  return `${String(h).padStart(2, '0')}:${m}:${s} ${ampm}`;
}

describe('formatClock', () => {
  test('formats morning time', () => {
    const date = new Date(2024, 0, 1, 9, 5, 3);
    expect(formatClock(date)).toBe('09:05:03 am');
  });

  test('formats afternoon time', () => {
    const date = new Date(2024, 0, 1, 14, 30, 45);
    expect(formatClock(date)).toBe('02:30:45 pm');
  });

  test('formats noon as 12:00:00 pm', () => {
    const date = new Date(2024, 0, 1, 12, 0, 0);
    expect(formatClock(date)).toBe('12:00:00 pm');
  });

  test('formats midnight as 12:00:00 am', () => {
    const date = new Date(2024, 0, 1, 0, 0, 0);
    expect(formatClock(date)).toBe('12:00:00 am');
  });

  test('formats 11:59:59 PM', () => {
    const date = new Date(2024, 0, 1, 23, 59, 59);
    expect(formatClock(date)).toBe('11:59:59 pm');
  });

  test('pads single-digit hours', () => {
    const date = new Date(2024, 0, 1, 1, 0, 0);
    expect(formatClock(date)).toBe('01:00:00 am');
  });
});
