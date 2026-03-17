/**
 * Unit tests for ISO 8601 to UTC epoch conversion
 * Mirrors: src/helloclub.cpp — parseISOToEpoch()
 *
 * Validates the manual UTC epoch calculation that replaces
 * the broken mktime-based approach (which got DST wrong).
 */

function parseISOToEpoch(isoDate) {
  if (!isoDate || isoDate.length < 19) return 0;

  const year = parseInt(isoDate.substring(0, 4));
  const mon  = parseInt(isoDate.substring(5, 7));
  const day  = parseInt(isoDate.substring(8, 10));
  const hour = parseInt(isoDate.substring(11, 13));
  const min  = parseInt(isoDate.substring(14, 16));
  const sec  = parseInt(isoDate.substring(17, 19));

  // Same algorithm as the C++ implementation
  let y = year;
  let m = mon;
  if (m <= 2) { y--; m += 12; }
  m -= 3; // Mar=0 .. Feb=11

  const days = 365 * (y - 1970)
    + Math.floor((y - 1968) / 4) - Math.floor((y - 1900) / 100) + Math.floor((y - 1600) / 400)
    + Math.floor((153 * m + 2) / 5) + day - 1
    + 59;

  return days * 86400 + hour * 3600 + min * 60 + sec;
}

// Reference: use JS Date.UTC for ground truth
function jsEpoch(iso) {
  return Math.floor(new Date(iso.endsWith('Z') ? iso : iso + 'Z').getTime() / 1000);
}

describe('parseISOToEpoch', () => {
  describe('known dates', () => {
    test('Unix epoch', () => {
      expect(parseISOToEpoch('1970-01-01T00:00:00Z')).toBe(0);
    });

    test('2000-01-01 midnight', () => {
      expect(parseISOToEpoch('2000-01-01T00:00:00Z')).toBe(jsEpoch('2000-01-01T00:00:00Z'));
    });

    test('2024-01-15 14:30:00', () => {
      const iso = '2024-01-15T14:30:00Z';
      expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
    });

    test('2026-03-17 05:30:00 (current context date)', () => {
      const iso = '2026-03-17T05:30:00Z';
      expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
    });
  });

  describe('DST transition dates (NZ Pacific/Auckland)', () => {
    // These are the dates where the old mktime approach would fail

    test('NZ DST start 2025: Sep 28 02:00 -> 03:00', () => {
      const iso = '2025-09-28T14:00:00Z'; // 3am NZDT
      expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
    });

    test('NZ DST end 2026: Apr 5 03:00 -> 02:00', () => {
      const iso = '2026-04-05T14:00:00Z'; // 2am NZST
      expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
    });

    test('during NZDT (UTC+13)', () => {
      const iso = '2026-03-17T17:30:00Z'; // 6:30am NZDT
      expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
    });

    test('during NZST (UTC+12)', () => {
      const iso = '2026-07-15T21:00:00Z'; // 9am NZST
      expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
    });
  });

  describe('leap years', () => {
    test('2024 leap year Feb 29', () => {
      const iso = '2024-02-29T12:00:00Z';
      expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
    });

    test('2024 Mar 1 (day after leap day)', () => {
      const iso = '2024-03-01T00:00:00Z';
      expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
    });

    test('2100 NOT a leap year', () => {
      const iso = '2100-03-01T00:00:00Z';
      expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
    });

    test('2000 IS a leap year (divisible by 400)', () => {
      const iso = '2000-02-29T23:59:59Z';
      expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
    });
  });

  describe('edge cases', () => {
    test('end of year', () => {
      const iso = '2025-12-31T23:59:59Z';
      expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
    });

    test('start of year', () => {
      const iso = '2025-01-01T00:00:00Z';
      expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
    });

    test('too short string returns 0', () => {
      expect(parseISOToEpoch('2025-01-01')).toBe(0);
    });

    test('empty string returns 0', () => {
      expect(parseISOToEpoch('')).toBe(0);
    });

    test('null returns 0', () => {
      expect(parseISOToEpoch(null)).toBe(0);
    });
  });

  describe('systematic validation across months', () => {
    // Test first day of every month in 2026
    const months = [
      '2026-01-01', '2026-02-01', '2026-03-01', '2026-04-01',
      '2026-05-01', '2026-06-01', '2026-07-01', '2026-08-01',
      '2026-09-01', '2026-10-01', '2026-11-01', '2026-12-01',
    ];

    months.forEach(date => {
      test(`${date}T00:00:00Z`, () => {
        const iso = `${date}T00:00:00Z`;
        expect(parseISOToEpoch(iso)).toBe(jsEpoch(iso));
      });
    });
  });

  describe('Hello Club realistic events', () => {
    test('Tuesday evening badminton 5:30pm-7:30pm NZDT', () => {
      // 5:30pm NZDT = 4:30am UTC
      const start = '2026-03-17T04:30:00Z';
      const end   = '2026-03-17T06:30:00Z';
      expect(parseISOToEpoch(start)).toBe(jsEpoch(start));
      expect(parseISOToEpoch(end)).toBe(jsEpoch(end));
      expect(parseISOToEpoch(end) - parseISOToEpoch(start)).toBe(7200); // 2 hours
    });

    test('Saturday morning 9am-12pm NZST (winter)', () => {
      // 9am NZST = 9pm previous day UTC
      const start = '2026-06-19T21:00:00Z';
      const end   = '2026-06-20T00:00:00Z';
      expect(parseISOToEpoch(start)).toBe(jsEpoch(start));
      expect(parseISOToEpoch(end)).toBe(jsEpoch(end));
      expect(parseISOToEpoch(end) - parseISOToEpoch(start)).toBe(10800); // 3 hours
    });
  });
});
