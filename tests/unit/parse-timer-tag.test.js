/**
 * Unit tests for parseTimerTag() — Hello Club event description parser
 * Source: test-server/server.js:88-122
 * Mirror of: src/helloclub.cpp parseTimerTag()
 */

// Default duration used when tag says "enabled" or omits duration
let hcDefaultDuration = 12;

function parseTimerTag(description) {
  if (!description) return null;
  const lower = description.toLowerCase();
  const tagPos = lower.indexOf('timer:');
  if (tagPos === -1) return null;

  let value = description.substring(tagPos + 6).trim();
  const nlPos = value.indexOf('\n');
  if (nlPos > 0) value = value.substring(0, nlPos);
  value = value.trim();
  const lowerValue = value.toLowerCase();

  let duration = hcDefaultDuration;
  let rounds = 0; // 0 = continuous

  if (lowerValue.startsWith('enabled') || value === '') {
    return { duration, rounds };
  }

  const minMatch = lowerValue.match(/(\d+)\s*min/);
  if (minMatch) {
    const val = parseInt(minMatch[1]);
    if (val >= 1 && val <= 120) duration = val;
  }

  const roundMatch = lowerValue.match(/(\d+)\s*round/);
  if (roundMatch) {
    const val = parseInt(roundMatch[1]);
    if (val >= 1 && val <= 20) rounds = val;
  }

  return { duration, rounds };
}

describe('parseTimerTag', () => {
  beforeEach(() => {
    hcDefaultDuration = 12;
  });

  describe('no tag', () => {
    test('returns null for empty/null description', () => {
      expect(parseTimerTag(null)).toBeNull();
      expect(parseTimerTag(undefined)).toBeNull();
      expect(parseTimerTag('')).toBeNull();
    });

    test('returns null when no timer: tag present', () => {
      expect(parseTimerTag('Regular badminton session')).toBeNull();
      expect(parseTimerTag('Booking for court 1')).toBeNull();
    });
  });

  describe('enabled tag', () => {
    test('timer: enabled uses default duration, continuous', () => {
      const result = parseTimerTag('timer: enabled');
      expect(result).toEqual({ duration: 12, rounds: 0 });
    });

    test('timer: enabled with surrounding text', () => {
      const result = parseTimerTag('Weekly session\ntimer: enabled\nSome notes');
      expect(result).toEqual({ duration: 12, rounds: 0 });
    });

    test('empty value after timer: uses defaults', () => {
      const result = parseTimerTag('timer: ');
      expect(result).toEqual({ duration: 12, rounds: 0 });
    });
  });

  describe('duration parsing', () => {
    test('parses Xmin format', () => {
      expect(parseTimerTag('timer: 20min')).toEqual({ duration: 20, rounds: 0 });
      expect(parseTimerTag('timer: 15min')).toEqual({ duration: 15, rounds: 0 });
      expect(parseTimerTag('timer: 1min')).toEqual({ duration: 1, rounds: 0 });
    });

    test('parses with space before min', () => {
      expect(parseTimerTag('timer: 20 min')).toEqual({ duration: 20, rounds: 0 });
    });

    test('clamps duration to valid range (1-120)', () => {
      expect(parseTimerTag('timer: 0min')).toEqual({ duration: 12, rounds: 0 }); // Falls through, uses default
      expect(parseTimerTag('timer: 121min')).toEqual({ duration: 12, rounds: 0 }); // Out of range
      expect(parseTimerTag('timer: 120min')).toEqual({ duration: 120, rounds: 0 }); // Max valid
    });
  });

  describe('round parsing', () => {
    test('parses Xrounds format', () => {
      expect(parseTimerTag('timer: 20min 3rounds')).toEqual({ duration: 20, rounds: 3 });
    });

    test('parses with space before rounds', () => {
      expect(parseTimerTag('timer: 20min 3 rounds')).toEqual({ duration: 20, rounds: 3 });
    });

    test('singular "round" works', () => {
      expect(parseTimerTag('timer: 20min 1round')).toEqual({ duration: 20, rounds: 1 });
    });

    test('clamps rounds to valid range (1-20)', () => {
      expect(parseTimerTag('timer: 20min 0rounds')).toEqual({ duration: 20, rounds: 0 }); // 0 stays as continuous
      expect(parseTimerTag('timer: 20min 21rounds')).toEqual({ duration: 20, rounds: 0 }); // Out of range
      expect(parseTimerTag('timer: 20min 20rounds')).toEqual({ duration: 20, rounds: 20 }); // Max valid
    });
  });

  describe('case insensitivity', () => {
    test('Timer: tag is case insensitive', () => {
      expect(parseTimerTag('Timer: 20min')).toEqual({ duration: 20, rounds: 0 });
      expect(parseTimerTag('TIMER: 20min')).toEqual({ duration: 20, rounds: 0 });
    });

    test('min/rounds keywords are case insensitive', () => {
      expect(parseTimerTag('timer: 20MIN 3ROUNDS')).toEqual({ duration: 20, rounds: 3 });
    });
  });

  describe('multiline descriptions', () => {
    test('only parses first line after timer:', () => {
      const result = parseTimerTag('timer: 20min\nignored: 30min');
      expect(result).toEqual({ duration: 20, rounds: 0 });
    });

    test('timer: tag on later line', () => {
      const result = parseTimerTag('Badminton night\ntimer: 15min 2rounds\nHave fun!');
      expect(result).toEqual({ duration: 15, rounds: 2 });
    });
  });

  describe('respects hcDefaultDuration', () => {
    test('enabled tag uses configured default', () => {
      hcDefaultDuration = 20;
      expect(parseTimerTag('timer: enabled')).toEqual({ duration: 20, rounds: 0 });
    });
  });
});
