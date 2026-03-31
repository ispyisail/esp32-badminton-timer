/**
 * Unit tests for remote log ring buffer
 * Mirrors: src/remotelog.cpp — ring buffer with JSON serialization
 */

const RLOG_MAX_ENTRIES = 40;
const RLOG_MSG_LEN = 100;

class RemoteLog {
  constructor() {
    this.entries = [];
    this.head = 0;
    this.count = 0;
    this.seq = 0;
    this.buffer = new Array(RLOG_MAX_ENTRIES).fill(null).map(() => ({
      timestamp: 0,
      message: '',
    }));
  }

  log(timestamp, message) {
    this.buffer[this.head] = {
      timestamp,
      message: message.substring(0, RLOG_MSG_LEN - 1),
    };
    this.head = (this.head + 1) % RLOG_MAX_ENTRIES;
    if (this.count < RLOG_MAX_ENTRIES) this.count++;
    this.seq++;
  }

  getEntries() {
    const result = [];
    const start = this.count < RLOG_MAX_ENTRIES ? 0 : this.head;
    for (let i = 0; i < this.count; i++) {
      const idx = (start + i) % RLOG_MAX_ENTRIES;
      result.push({ ...this.buffer[idx] });
    }
    return result;
  }

  getSeq() { return this.seq; }
  getCount() { return this.count; }
}

describe('Remote Log Ring Buffer', () => {
  let log;

  beforeEach(() => {
    log = new RemoteLog();
  });

  test('starts empty', () => {
    expect(log.getCount()).toBe(0);
    expect(log.getSeq()).toBe(0);
    expect(log.getEntries()).toHaveLength(0);
  });

  test('logs a single entry', () => {
    log.log(1000, 'test message');
    expect(log.getCount()).toBe(1);
    expect(log.getSeq()).toBe(1);
    const entries = log.getEntries();
    expect(entries).toHaveLength(1);
    expect(entries[0].timestamp).toBe(1000);
    expect(entries[0].message).toBe('test message');
  });

  test('logs multiple entries in order', () => {
    log.log(100, 'first');
    log.log(200, 'second');
    log.log(300, 'third');
    const entries = log.getEntries();
    expect(entries).toHaveLength(3);
    expect(entries[0].message).toBe('first');
    expect(entries[1].message).toBe('second');
    expect(entries[2].message).toBe('third');
  });

  test('wraps around at max entries', () => {
    for (let i = 0; i < RLOG_MAX_ENTRIES + 5; i++) {
      log.log(i * 100, `msg ${i}`);
    }
    expect(log.getCount()).toBe(RLOG_MAX_ENTRIES);
    expect(log.getSeq()).toBe(RLOG_MAX_ENTRIES + 5);

    const entries = log.getEntries();
    expect(entries).toHaveLength(RLOG_MAX_ENTRIES);
    // Oldest should be msg 5 (first 5 got overwritten)
    expect(entries[0].message).toBe('msg 5');
    // Newest should be msg 44
    expect(entries[RLOG_MAX_ENTRIES - 1].message).toBe(`msg ${RLOG_MAX_ENTRIES + 4}`);
  });

  test('entries returned oldest-to-newest after wrap', () => {
    for (let i = 0; i < RLOG_MAX_ENTRIES * 2; i++) {
      log.log(i, `msg ${i}`);
    }
    const entries = log.getEntries();
    for (let i = 1; i < entries.length; i++) {
      expect(entries[i].timestamp).toBeGreaterThan(entries[i - 1].timestamp);
    }
  });

  test('truncates long messages', () => {
    const longMsg = 'a'.repeat(200);
    log.log(1000, longMsg);
    const entries = log.getEntries();
    expect(entries[0].message.length).toBeLessThan(RLOG_MSG_LEN);
  });

  test('sequence counter increments monotonically', () => {
    for (let i = 0; i < 100; i++) {
      log.log(i, `msg ${i}`);
      expect(log.getSeq()).toBe(i + 1);
    }
  });

  test('count caps at max entries', () => {
    for (let i = 0; i < RLOG_MAX_ENTRIES * 3; i++) {
      log.log(i, `msg`);
    }
    expect(log.getCount()).toBe(RLOG_MAX_ENTRIES);
  });
});
