/**
 * Unit tests for the Siren state machine
 * Mirrors: src/siren.cpp — non-blocking relay control with safety timeout
 */

const SAFETY_TIMEOUT_MS = 5000;

class Siren {
  constructor() {
    this.blastLength = 1000;
    this.blastPause = 1000;
    this.active = false;
    this.blastsRemaining = 0;
    this.lastActionTime = 0;
    this.relayOn = false;
    this._now = 0;
    this._pinState = false; // LOW = false, HIGH = true
    this._log = [];
  }

  setNow(ms) { this._now = ms; }
  advanceTime(ms) { this._now += ms; }

  // Simulate digitalWrite
  _digitalWrite(state) {
    this._pinState = state;
    this._log.push({ time: this._now, state });
  }

  update() {
    if (!this.active) {
      if (this.relayOn) {
        this._digitalWrite(false);
        this.relayOn = false;
      }
      return;
    }

    const now = this._now;

    // Safety timeout
    if (this.relayOn && (now - this.lastActionTime >= SAFETY_TIMEOUT_MS)) {
      this._digitalWrite(false);
      this.relayOn = false;
      this.lastActionTime = now;
      this.blastsRemaining--;
      if (this.blastsRemaining <= 0) {
        this.active = false;
      }
      return;
    }

    if (this.relayOn) {
      if (now - this.lastActionTime >= this.blastLength) {
        this._digitalWrite(false);
        this.relayOn = false;
        this.lastActionTime = now;
        this.blastsRemaining--;
        if (this.blastsRemaining <= 0) {
          this.active = false;
        }
      }
    } else {
      if (this.blastsRemaining > 0) {
        if (now - this.lastActionTime >= this.blastPause) {
          this._digitalWrite(true);
          this.relayOn = true;
          this.lastActionTime = now;
        }
      } else {
        this.active = false;
      }
    }
  }

  start(blasts) {
    if (this.active || blasts <= 0) return;
    this.blastsRemaining = blasts;
    this.active = true;
    this.relayOn = false;
    this.lastActionTime = this._now - this.blastPause; // Immediate first blast
  }

  stop() {
    this.active = false;
    this.blastsRemaining = 0;
    this._digitalWrite(false);
    this.relayOn = false;
  }
}

describe('Siren State Machine', () => {
  let siren;

  beforeEach(() => {
    siren = new Siren();
    siren.setNow(10000);
  });

  describe('initial state', () => {
    test('starts inactive with relay off', () => {
      expect(siren.active).toBe(false);
      expect(siren.relayOn).toBe(false);
      expect(siren._pinState).toBe(false);
    });

    test('update does nothing when inactive', () => {
      siren.update();
      expect(siren.active).toBe(false);
      expect(siren._pinState).toBe(false);
    });
  });

  describe('single blast', () => {
    test('starts relay immediately on start(1)', () => {
      siren.start(1);
      siren.update();
      expect(siren.relayOn).toBe(true);
      expect(siren._pinState).toBe(true);
    });

    test('turns off after blastLength', () => {
      siren.start(1);
      siren.update(); // Relay ON
      siren.advanceTime(1000); // blastLength elapsed
      siren.update();
      expect(siren.relayOn).toBe(false);
      expect(siren._pinState).toBe(false);
      expect(siren.active).toBe(false);
    });

    test('stays on before blastLength elapsed', () => {
      siren.start(1);
      siren.update();
      siren.advanceTime(500); // Half of blastLength
      siren.update();
      expect(siren.relayOn).toBe(true);
      expect(siren.active).toBe(true);
    });
  });

  describe('multiple blasts', () => {
    test('2-blast sequence: ON-OFF-ON-OFF', () => {
      siren.blastLength = 500;
      siren.blastPause = 300;
      siren.start(2);

      // First blast starts immediately
      siren.update();
      expect(siren.relayOn).toBe(true);
      expect(siren.blastsRemaining).toBe(2);

      // First blast ends after 500ms
      siren.advanceTime(500);
      siren.update();
      expect(siren.relayOn).toBe(false);
      expect(siren.blastsRemaining).toBe(1);
      expect(siren.active).toBe(true);

      // Pause — too early for next blast
      siren.advanceTime(200);
      siren.update();
      expect(siren.relayOn).toBe(false);

      // Pause complete — second blast starts
      siren.advanceTime(100);
      siren.update();
      expect(siren.relayOn).toBe(true);

      // Second blast ends
      siren.advanceTime(500);
      siren.update();
      expect(siren.relayOn).toBe(false);
      expect(siren.active).toBe(false);
    });

    test('3-blast round-end siren completes', () => {
      siren.start(3);
      let blastCount = 0;

      for (let i = 0; i < 100; i++) { // Step through time
        siren.advanceTime(100);
        const waOn = siren.relayOn;
        siren.update();
        if (!waOn && siren.relayOn) blastCount++;
        if (!siren.active) break;
      }

      expect(blastCount).toBe(3);
      expect(siren.active).toBe(false);
      expect(siren.relayOn).toBe(false);
    });
  });

  describe('stop', () => {
    test('immediately stops and turns off relay', () => {
      siren.start(3);
      siren.update(); // Relay ON
      siren.stop();
      expect(siren.active).toBe(false);
      expect(siren.relayOn).toBe(false);
      expect(siren._pinState).toBe(false);
    });

    test('cannot restart while active (must stop first)', () => {
      siren.start(2);
      siren.update();
      siren.start(5); // Should be ignored
      expect(siren.blastsRemaining).toBe(2);
    });

    test('can restart after stop', () => {
      siren.start(2);
      siren.update();
      siren.stop();
      siren.start(3);
      expect(siren.blastsRemaining).toBe(3);
      expect(siren.active).toBe(true);
    });
  });

  describe('safety timeout', () => {
    test('forces relay off after 5 seconds', () => {
      siren.blastLength = 1000;
      siren.start(2);
      siren.update(); // Relay ON

      // Simulate blocked loop — 6 seconds pass without update
      siren.advanceTime(6000);
      siren.update();

      // Safety should have forced relay off
      expect(siren.relayOn).toBe(false);
      expect(siren._pinState).toBe(false);
      // One blast consumed by the safety timeout
      expect(siren.blastsRemaining).toBe(1);
    });

    test('sequence continues after safety timeout', () => {
      siren.blastLength = 1000;
      siren.blastPause = 500;
      siren.start(2);
      siren.update(); // Blast 1 ON

      // Blocked loop — safety triggers
      siren.advanceTime(6000);
      siren.update(); // Safety forces OFF, blastsRemaining = 1

      expect(siren.active).toBe(true);
      expect(siren.blastsRemaining).toBe(1);

      // After pause, next blast fires normally
      siren.advanceTime(500);
      siren.update();
      expect(siren.relayOn).toBe(true);

      // Normal blast end
      siren.advanceTime(1000);
      siren.update();
      expect(siren.relayOn).toBe(false);
      expect(siren.active).toBe(false);
    });

    test('safety timeout on last blast ends sequence', () => {
      siren.start(1);
      siren.update(); // Relay ON

      siren.advanceTime(6000);
      siren.update();

      expect(siren.relayOn).toBe(false);
      expect(siren.active).toBe(false);
    });

    test('does not trigger before 5 seconds', () => {
      siren.blastLength = 4000; // Long blast, but under safety limit
      siren.start(1);
      siren.update(); // Relay ON

      siren.advanceTime(4000);
      siren.update();

      // Normal blast end, not safety timeout
      expect(siren.relayOn).toBe(false);
      expect(siren.active).toBe(false);
    });

    test('exactly at 5 seconds triggers safety', () => {
      siren.blastLength = 10000; // Longer than safety
      siren.start(1);
      siren.update();

      siren.advanceTime(5000); // Exactly at safety threshold
      siren.update();

      expect(siren.relayOn).toBe(false);
    });
  });

  describe('inactive relay guard', () => {
    test('forces relay off if inactive but relayOn is true', () => {
      // Simulate corrupted state
      siren.active = false;
      siren.relayOn = true;
      siren._pinState = true;

      siren.update();

      expect(siren.relayOn).toBe(false);
      expect(siren._pinState).toBe(false);
    });
  });

  describe('configurable timing', () => {
    test('respects custom blast length', () => {
      siren.blastLength = 2000;
      siren.start(1);
      siren.update();

      siren.advanceTime(1500);
      siren.update();
      expect(siren.relayOn).toBe(true); // Still on

      siren.advanceTime(500);
      siren.update();
      expect(siren.relayOn).toBe(false); // Now off
    });

    test('respects custom pause length', () => {
      siren.blastLength = 500;
      siren.blastPause = 2000;
      siren.start(2);
      siren.update(); // Blast 1

      siren.advanceTime(500);
      siren.update(); // Blast 1 ends

      // Pause not yet complete
      siren.advanceTime(1500);
      siren.update();
      expect(siren.relayOn).toBe(false);

      // Pause complete
      siren.advanceTime(500);
      siren.update();
      expect(siren.relayOn).toBe(true);
    });
  });

  describe('edge cases', () => {
    test('start with 0 blasts is ignored', () => {
      siren.start(0);
      expect(siren.active).toBe(false);
    });

    test('start with negative blasts is ignored', () => {
      siren.start(-1);
      expect(siren.active).toBe(false);
    });

    test('rapid update calls do not double-trigger', () => {
      siren.start(1);
      siren.update();
      expect(siren.relayOn).toBe(true);

      // Multiple updates at same time
      siren.update();
      siren.update();
      siren.update();
      expect(siren.relayOn).toBe(true);
      expect(siren.blastsRemaining).toBe(1); // Not decremented
    });
  });
});
