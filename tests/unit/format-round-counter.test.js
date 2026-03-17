/**
 * Unit tests for formatRoundCounter() — displays round information
 * Source: data/script.js:174-176
 */

describe('formatRoundCounter', () => {
  // The function depends on global `continuousMode`, so we test both modes

  let continuousMode;

  function formatRoundCounter(current, total) {
    return continuousMode ? `Round ${current}` : `Round ${current} of ${total}`;
  }

  beforeEach(() => {
    continuousMode = false;
  });

  test('normal mode shows "Round X of Y"', () => {
    continuousMode = false;
    expect(formatRoundCounter(1, 3)).toBe('Round 1 of 3');
    expect(formatRoundCounter(2, 3)).toBe('Round 2 of 3');
    expect(formatRoundCounter(3, 3)).toBe('Round 3 of 3');
  });

  test('continuous mode shows only "Round X"', () => {
    continuousMode = true;
    expect(formatRoundCounter(1, 3)).toBe('Round 1');
    expect(formatRoundCounter(5, 3)).toBe('Round 5');
    expect(formatRoundCounter(99, 3)).toBe('Round 99');
  });

  test('single round game', () => {
    continuousMode = false;
    expect(formatRoundCounter(1, 1)).toBe('Round 1 of 1');
  });

  test('max rounds', () => {
    continuousMode = false;
    expect(formatRoundCounter(20, 20)).toBe('Round 20 of 20');
  });
});
