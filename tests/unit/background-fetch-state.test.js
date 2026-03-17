/**
 * Unit tests for the Hello Club background fetch state machine
 * Mirrors: src/main.cpp — hcFetchInProgress / hcFetchResultReady flag transitions
 *
 * The fetch runs on a FreeRTOS background task. The main loop only interacts
 * via three volatile flags. This tests the state transitions are correct.
 */

class BackgroundFetchStateMachine {
  constructor() {
    this.fetchInProgress = false;
    this.fetchResultReady = false;
    this.fetchResultSuccess = false;
    this.lastPoll = 0;
    this.pollFailed = false;
    this.pollInterval = 3600000;    // 1 hour
    this.retryInterval = 300000;    // 5 minutes
    this.fetchStarted = false;      // Track if we launched a fetch
    this.resultHandled = false;     // Track if we handled a result
  }

  // Simulate checkHelloClubPoll() from main loop
  check(now) {
    this.fetchStarted = false;
    this.resultHandled = false;

    // Check if background fetch completed
    if (this.fetchResultReady) {
      this.fetchResultReady = false;
      this.lastPoll = now;
      this.resultHandled = true;

      if (this.fetchResultSuccess) {
        this.pollFailed = false;
      } else {
        this.pollFailed = true;
      }
      return;
    }

    // Don't start if one is already running
    if (this.fetchInProgress) {
      return;
    }

    const interval = this.pollFailed ? this.retryInterval : this.pollInterval;
    if (this.lastPoll > 0 && now - this.lastPoll < interval) {
      return;
    }

    // Launch background fetch
    this.fetchInProgress = true;
    this.fetchResultReady = false;
    this.fetchStarted = true;
  }

  // Simulate background task completing
  completeFetch(success) {
    this.fetchResultSuccess = success;
    this.fetchResultReady = true;
    this.fetchInProgress = false;
  }
}

describe('Background Fetch State Machine', () => {
  let sm;

  beforeEach(() => {
    sm = new BackgroundFetchStateMachine();
  });

  describe('initial fetch', () => {
    test('launches fetch on first check (lastPoll=0)', () => {
      sm.check(1000);
      expect(sm.fetchStarted).toBe(true);
      expect(sm.fetchInProgress).toBe(true);
    });

    test('does not launch while fetch is in progress', () => {
      sm.check(1000); // Launch
      sm.check(2000); // Should not re-launch
      expect(sm.fetchStarted).toBe(false);
      expect(sm.fetchInProgress).toBe(true);
    });
  });

  describe('successful fetch', () => {
    test('handles success result on next check', () => {
      sm.check(1000); // Launch
      sm.completeFetch(true); // Background completes

      sm.check(2000); // Process result
      expect(sm.resultHandled).toBe(true);
      expect(sm.pollFailed).toBe(false);
      expect(sm.lastPoll).toBe(2000);
    });

    test('after success, waits full poll interval', () => {
      sm.check(0); // Launch
      sm.completeFetch(true);
      sm.check(1000); // Process result

      // Too early for next poll
      sm.check(1000 + 1800000); // 30 min later
      expect(sm.fetchStarted).toBe(false);

      // After full interval
      sm.check(1000 + 3600000); // 1 hour later
      expect(sm.fetchStarted).toBe(true);
    });
  });

  describe('failed fetch', () => {
    test('handles failure result', () => {
      sm.check(0);
      sm.completeFetch(false);

      sm.check(1000);
      expect(sm.resultHandled).toBe(true);
      expect(sm.pollFailed).toBe(true);
    });

    test('after failure, retries at shorter interval', () => {
      sm.check(0);
      sm.completeFetch(false);
      sm.check(1000); // Process failure

      // Too early for retry (5 min interval)
      sm.check(1000 + 60000); // 1 min later
      expect(sm.fetchStarted).toBe(false);

      // After retry interval
      sm.check(1000 + 300000); // 5 min later
      expect(sm.fetchStarted).toBe(true);
    });

    test('success after failure resets to normal interval', () => {
      // First: fail
      sm.check(0);
      sm.completeFetch(false);
      sm.check(1000);
      expect(sm.pollFailed).toBe(true);

      // Retry: succeed
      sm.check(1000 + 300000);
      sm.completeFetch(true);
      sm.check(1000 + 301000);
      expect(sm.pollFailed).toBe(false);

      // Should wait full hour now, not 5 min
      sm.check(1000 + 301000 + 300000); // 5 min later
      expect(sm.fetchStarted).toBe(false);

      sm.check(1000 + 301000 + 3600000); // 1 hour later
      expect(sm.fetchStarted).toBe(true);
    });
  });

  describe('flag consistency', () => {
    test('fetchInProgress and fetchResultReady are never both true', () => {
      sm.check(0);
      expect(sm.fetchInProgress).toBe(true);
      expect(sm.fetchResultReady).toBe(false);

      sm.completeFetch(true);
      expect(sm.fetchInProgress).toBe(false);
      expect(sm.fetchResultReady).toBe(true);

      sm.check(1000);
      expect(sm.fetchInProgress).toBe(false);
      expect(sm.fetchResultReady).toBe(false);
    });

    test('result flag is cleared after being consumed', () => {
      sm.check(0);
      sm.completeFetch(true);
      sm.check(1000); // Consume result

      // Second check should not see the result again
      sm.check(2000);
      expect(sm.resultHandled).toBe(false);
    });
  });

  describe('rapid polling', () => {
    test('multiple checks during in-flight fetch all no-op', () => {
      sm.check(0); // Launch
      for (let t = 100; t < 5000; t += 100) {
        sm.check(t);
        expect(sm.fetchStarted).toBe(false);
      }
      expect(sm.fetchInProgress).toBe(true);
    });

    test('result is only processed once even with rapid checks', () => {
      sm.check(0);
      sm.completeFetch(true);

      sm.check(1000); // First check sees result
      expect(sm.resultHandled).toBe(true);

      sm.check(1001);
      expect(sm.resultHandled).toBe(false); // Already consumed
    });
  });
});
