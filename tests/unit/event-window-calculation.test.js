/**
 * Unit tests for event window calculation (short booking fix)
 * Mirrors: src/main.cpp — activeEventEndTime calculation
 *
 * For short/instant bookings (endTime ≈ startTime), the timer duration
 * is what matters, not the booking endTime. The activeEventEndTime is
 * calculated as: max(bookingEndTime, startTime + timerPlayDuration)
 */

/**
 * Replicates the activeEventEndTime calculation from main.cpp
 * when an event auto-triggers.
 */
function calculateActiveEventEndTime(evt) {
  let timerPlayDurationSec;
  if (evt.numRounds > 0) {
    timerPlayDurationSec = evt.numRounds * evt.durationMin * 60;
  } else {
    // Continuous mode — use single round duration as minimum
    timerPlayDurationSec = evt.durationMin * 60;
  }

  const timerEnd = evt.startTime + timerPlayDurationSec;

  // Use the later of booking end or timer play end
  return Math.max(evt.endTime, timerEnd);
}

describe('Event Window Calculation', () => {
  describe('normal bookings (endTime > play duration)', () => {
    test('2-hour booking with 60min play time — uses booking endTime', () => {
      const result = calculateActiveEventEndTime({
        startTime: 1000,
        endTime: 1000 + 7200,  // 2 hours
        durationMin: 20,
        numRounds: 3,          // 60 min total
      });
      expect(result).toBe(1000 + 7200); // booking end wins
    });

    test('booking exactly matches play time', () => {
      const result = calculateActiveEventEndTime({
        startTime: 1000,
        endTime: 1000 + 3600,  // 60 min booking
        durationMin: 20,
        numRounds: 3,          // 60 min play
      });
      expect(result).toBe(1000 + 3600); // equal — both are the same
    });
  });

  describe('short bookings (endTime ≈ startTime)', () => {
    test('1-minute booking with 60min play — uses timer play end', () => {
      const result = calculateActiveEventEndTime({
        startTime: 1000,
        endTime: 1000 + 60,    // 1 minute booking!
        durationMin: 20,
        numRounds: 3,          // 60 min play
      });
      expect(result).toBe(1000 + 3600); // timer play end wins
    });

    test('instant booking (endTime = startTime) with play time', () => {
      const result = calculateActiveEventEndTime({
        startTime: 1000,
        endTime: 1000,         // zero-length booking
        durationMin: 20,
        numRounds: 3,
      });
      expect(result).toBe(1000 + 3600); // timer play end = 60 min after start
    });

    test('endTime before startTime (edge case)', () => {
      const result = calculateActiveEventEndTime({
        startTime: 1000,
        endTime: 900,          // endTime before start (shouldn't happen, but handle it)
        durationMin: 20,
        numRounds: 3,
      });
      expect(result).toBe(1000 + 3600); // timer play end wins
    });
  });

  describe('continuous mode', () => {
    test('continuous with long booking — uses booking endTime', () => {
      const result = calculateActiveEventEndTime({
        startTime: 1000,
        endTime: 1000 + 7200,  // 2 hour booking
        durationMin: 20,
        numRounds: 0,          // continuous
      });
      expect(result).toBe(1000 + 7200); // booking > 1 round duration
    });

    test('continuous with short booking — uses single round duration', () => {
      const result = calculateActiveEventEndTime({
        startTime: 1000,
        endTime: 1000 + 60,    // 1 minute booking
        durationMin: 20,
        numRounds: 0,          // continuous
      });
      expect(result).toBe(1000 + 1200); // 20 min = 1200 sec
    });
  });

  describe('realistic scenarios', () => {
    test('Hello Club short booking: 5:30pm start, 5:31pm end, 3x20min timer', () => {
      const start = 1773721800;  // 5:30pm
      const end = start + 60;    // 5:31pm
      const result = calculateActiveEventEndTime({
        startTime: start,
        endTime: end,
        durationMin: 20,
        numRounds: 3,
      });
      // Should allow full 60 min of play
      expect(result).toBe(start + 3600);
      expect(result).toBeGreaterThan(end);
    });

    test('Hello Club normal booking: 5:30pm-7:30pm, 3x20min timer', () => {
      const start = 1773721800;
      const end = start + 7200;  // 2 hours
      const result = calculateActiveEventEndTime({
        startTime: start,
        endTime: end,
        durationMin: 20,
        numRounds: 3,
      });
      // Booking window is longer, use it
      expect(result).toBe(end);
    });
  });
});
