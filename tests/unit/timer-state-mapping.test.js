/**
 * Unit tests for timer state string mapping
 * Mirrors: src/main.cpp — sendStateUpdate() state string mapping
 *
 * The timer has 4 states internally (0-3). The WebSocket protocol
 * sends these as string names. Bug #8 was that FINISHED (state 3)
 * was being sent as empty string.
 */

const STATE_MAP = {
  0: 'IDLE',
  1: 'RUNNING',
  2: 'PAUSED',
  3: 'FINISHED',
};

function mapTimerState(stateNum) {
  return STATE_MAP[stateNum] || 'UNKNOWN';
}

describe('Timer State Mapping', () => {
  test('maps IDLE (0)', () => {
    expect(mapTimerState(0)).toBe('IDLE');
  });

  test('maps RUNNING (1)', () => {
    expect(mapTimerState(1)).toBe('RUNNING');
  });

  test('maps PAUSED (2)', () => {
    expect(mapTimerState(2)).toBe('PAUSED');
  });

  test('maps FINISHED (3)', () => {
    expect(mapTimerState(3)).toBe('FINISHED');
  });

  test('all 4 states are mapped', () => {
    expect(Object.keys(STATE_MAP)).toHaveLength(4);
  });

  test('unknown state returns UNKNOWN', () => {
    expect(mapTimerState(99)).toBe('UNKNOWN');
  });

  test('negative state returns UNKNOWN', () => {
    expect(mapTimerState(-1)).toBe('UNKNOWN');
  });
});
