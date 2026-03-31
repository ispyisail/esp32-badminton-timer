/**
 * Unit tests for mid-event boot recovery math
 * Mirrors: src/helloclub.cpp — checkMidEventRecovery()
 *
 * Tests the pure calculation logic that determines which round
 * we're in and how much time remains, given wall-clock elapsed time.
 */

/**
 * Replicates the C++ recovery calculation from checkMidEventRecovery():
 * - Only recovers events that have been triggered (auto-started before reboot)
 * - Uses play window (from timer: tag) instead of booking endTime
 * - For short bookings, play duration may exceed booking window
 */
function calculateRecovery(nowEpoch, startTime, endTime, durationMin, numRounds, triggered = true) {
  // Must be triggered — only recover events that were actually auto-started
  if (!triggered) {
    return { shouldRecover: false };
  }

  if (nowEpoch < startTime) {
    return { shouldRecover: false };
  }

  const elapsedSec = nowEpoch - startTime;
  const roundDurationSec = durationMin * 60;
  const isContinuous = numRounds === 0;

  // Calculate actual play window from timer duration (not booking endTime)
  let totalPlaySec;
  if (!isContinuous) {
    totalPlaySec = numRounds * roundDurationSec;
  } else {
    // Continuous — use the longer of booking duration or timer duration
    const bookingDuration = endTime > startTime ? endTime - startTime : 0;
    totalPlaySec = Math.max(bookingDuration, roundDurationSec);
  }

  if (elapsedSec >= totalPlaySec) {
    return { shouldRecover: false };
  }

  const currentRound = Math.floor(elapsedSec / roundDurationSec) + 1;
  const elapsedInRound = elapsedSec % roundDurationSec;
  const remainingMs = (roundDurationSec - elapsedInRound) * 1000;

  return {
    shouldRecover: true,
    currentRound,
    remainingMs,
    durationMin,
    numRounds,
  };
}

describe('Mid-Event Recovery Math', () => {
  const BASE_TIME = 1700000000; // Arbitrary epoch

  describe('basic recovery', () => {
    test('recovers at start of event', () => {
      const result = calculateRecovery(
        BASE_TIME,         // now = exactly at start
        BASE_TIME,         // startTime
        BASE_TIME + 7200,  // endTime = 2 hours later
        20,                // 20 min rounds
        3                  // 3 rounds
      );
      expect(result.shouldRecover).toBe(true);
      expect(result.currentRound).toBe(1);
      expect(result.remainingMs).toBe(20 * 60 * 1000);
    });

    test('recovers mid first round', () => {
      const result = calculateRecovery(
        BASE_TIME + 600,   // 10 minutes in
        BASE_TIME,
        BASE_TIME + 7200,
        20, 3
      );
      expect(result.shouldRecover).toBe(true);
      expect(result.currentRound).toBe(1);
      expect(result.remainingMs).toBe(10 * 60 * 1000); // 10 min remaining
    });

    test('recovers in second round', () => {
      const result = calculateRecovery(
        BASE_TIME + 1500,  // 25 minutes in (5 min into round 2)
        BASE_TIME,
        BASE_TIME + 7200,
        20, 3
      );
      expect(result.shouldRecover).toBe(true);
      expect(result.currentRound).toBe(2);
      expect(result.remainingMs).toBe(15 * 60 * 1000); // 15 min remaining
    });

    test('recovers near end of last round', () => {
      const result = calculateRecovery(
        BASE_TIME + 3540,  // 59 minutes in (19 min into round 3)
        BASE_TIME,
        BASE_TIME + 7200,
        20, 3
      );
      expect(result.shouldRecover).toBe(true);
      expect(result.currentRound).toBe(3);
      expect(result.remainingMs).toBe(1 * 60 * 1000); // 1 min remaining
    });
  });

  describe('no recovery cases', () => {
    test('before event start', () => {
      const result = calculateRecovery(
        BASE_TIME - 60,
        BASE_TIME,
        BASE_TIME + 7200,
        20, 3
      );
      expect(result.shouldRecover).toBe(false);
    });

    test('after event end', () => {
      const result = calculateRecovery(
        BASE_TIME + 7200,
        BASE_TIME,
        BASE_TIME + 7200,
        20, 3
      );
      expect(result.shouldRecover).toBe(false);
    });

    test('all rounds completed (within event window)', () => {
      // 3 rounds x 20 min = 60 min = 3600 sec
      // At 3600 sec, all rounds are done
      const result = calculateRecovery(
        BASE_TIME + 3600,
        BASE_TIME,
        BASE_TIME + 7200, // Event window extends beyond rounds
        20, 3
      );
      expect(result.shouldRecover).toBe(false);
    });

    test('well past all rounds', () => {
      const result = calculateRecovery(
        BASE_TIME + 5000,
        BASE_TIME,
        BASE_TIME + 7200,
        20, 3
      );
      expect(result.shouldRecover).toBe(false);
    });
  });

  describe('continuous mode (numRounds=0)', () => {
    test('recovers in continuous mode', () => {
      const result = calculateRecovery(
        BASE_TIME + 3000,  // 50 min in
        BASE_TIME,
        BASE_TIME + 7200,
        20, 0              // continuous
      );
      expect(result.shouldRecover).toBe(true);
      expect(result.currentRound).toBe(3); // 50 min / 20 min = round 3
      expect(result.remainingMs).toBe(10 * 60 * 1000); // 10 min left
    });

    test('continuous mode keeps going past normal finish', () => {
      const result = calculateRecovery(
        BASE_TIME + 5400,  // 90 min in
        BASE_TIME,
        BASE_TIME + 7200,
        20, 0
      );
      expect(result.shouldRecover).toBe(true);
      expect(result.currentRound).toBe(5); // 90/20 = 4.5, floor+1 = 5
      expect(result.remainingMs).toBe(10 * 60 * 1000);
    });
  });

  describe('edge cases', () => {
    test('exactly at round boundary', () => {
      // Exactly 20 min in = start of round 2
      const result = calculateRecovery(
        BASE_TIME + 1200,
        BASE_TIME,
        BASE_TIME + 7200,
        20, 3
      );
      expect(result.shouldRecover).toBe(true);
      expect(result.currentRound).toBe(2);
      expect(result.remainingMs).toBe(20 * 60 * 1000); // Full round
    });

    test('1 second before round ends', () => {
      const result = calculateRecovery(
        BASE_TIME + 1199,  // 19:59 into round 1
        BASE_TIME,
        BASE_TIME + 7200,
        20, 3
      );
      expect(result.shouldRecover).toBe(true);
      expect(result.currentRound).toBe(1);
      expect(result.remainingMs).toBe(1000); // 1 second
    });

    test('single round event', () => {
      const result = calculateRecovery(
        BASE_TIME + 300,
        BASE_TIME,
        BASE_TIME + 1200,
        20, 1
      );
      expect(result.shouldRecover).toBe(true);
      expect(result.currentRound).toBe(1);
      expect(result.remainingMs).toBe(15 * 60 * 1000);
    });

    test('single round completed', () => {
      const result = calculateRecovery(
        BASE_TIME + 1200, // exactly 20 min
        BASE_TIME,
        BASE_TIME + 3600,
        20, 1
      );
      expect(result.shouldRecover).toBe(false);
    });
  });

  describe('triggered flag requirement', () => {
    test('does NOT recover untriggered event', () => {
      const result = calculateRecovery(
        BASE_TIME + 600,
        BASE_TIME,
        BASE_TIME + 7200,
        20, 3,
        false // not triggered
      );
      expect(result.shouldRecover).toBe(false);
    });

    test('recovers triggered event at same point', () => {
      const result = calculateRecovery(
        BASE_TIME + 600,
        BASE_TIME,
        BASE_TIME + 7200,
        20, 3,
        true // triggered
      );
      expect(result.shouldRecover).toBe(true);
      expect(result.currentRound).toBe(1);
    });

    test('untriggered event mid-window still not recovered', () => {
      // Even though we're within the event window, if it wasn't
      // triggered before the reboot, don't recover
      const result = calculateRecovery(
        BASE_TIME + 1500,
        BASE_TIME,
        BASE_TIME + 7200,
        20, 3,
        false
      );
      expect(result.shouldRecover).toBe(false);
    });
  });

  describe('short booking recovery', () => {
    test('short booking: 1-min booking, 60-min play — recovers mid-play', () => {
      const start = BASE_TIME;
      const end = start + 60;  // 1-minute booking

      // 30 minutes into play (well past booking endTime)
      const result = calculateRecovery(
        start + 1800,  // 30 min in
        start,
        end,
        20, 3,  // 3 x 20min = 60min play
        true
      );
      expect(result.shouldRecover).toBe(true);
      expect(result.currentRound).toBe(2); // 30/20 = 1.5, floor+1 = 2
      expect(result.remainingMs).toBe(10 * 60 * 1000); // 10 min left in round
    });

    test('short booking: all rounds completed — no recovery', () => {
      const start = BASE_TIME;
      const end = start + 60;

      const result = calculateRecovery(
        start + 3600,  // 60 min = all 3 rounds done
        start,
        end,
        20, 3,
        true
      );
      expect(result.shouldRecover).toBe(false);
    });

    test('instant booking (endTime=startTime) still recovers', () => {
      const start = BASE_TIME;
      const result = calculateRecovery(
        start + 300,
        start,
        start,  // zero-length booking
        20, 3,
        true
      );
      expect(result.shouldRecover).toBe(true);
      expect(result.currentRound).toBe(1);
    });
  });

  describe('continuous mode with short bookings', () => {
    test('continuous, short booking: uses timer duration as minimum', () => {
      const start = BASE_TIME;
      const end = start + 60; // 1-min booking

      // Continuous mode, 20-min rounds
      // Play window: max(bookingDuration=60s, roundDuration=1200s) = 1200s
      const result = calculateRecovery(
        start + 600,  // 10 min in
        start,
        end,
        20, 0,  // continuous
        true
      );
      expect(result.shouldRecover).toBe(true);
    });

    test('continuous, short booking: past timer duration — no recovery', () => {
      const start = BASE_TIME;
      const end = start + 60;

      const result = calculateRecovery(
        start + 1200,  // exactly 20 min = round duration
        start,
        end,
        20, 0,
        true
      );
      expect(result.shouldRecover).toBe(false);
    });

    test('continuous, long booking: uses booking duration', () => {
      const start = BASE_TIME;
      const end = start + 7200; // 2 hour booking

      // max(bookingDuration=7200, roundDuration=1200) = 7200
      const result = calculateRecovery(
        start + 3600,  // 1 hour in
        start,
        end,
        20, 0,
        true
      );
      expect(result.shouldRecover).toBe(true);
    });
  });
});
