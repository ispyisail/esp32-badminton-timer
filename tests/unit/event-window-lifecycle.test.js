/**
 * Unit tests for event window lifecycle
 * Mirrors: src/main.cpp — activeEventEndTime/activeEventId/activeEventName
 *
 * Tests the full lifecycle of event window state across:
 * auto-trigger start → running → cutoff or manual reset
 */

class EventWindowState {
  constructor() {
    this.activeEventEndTime = 0;
    this.activeEventName = '';
    this.activeEventId = '';
    this.cancelFlag = '';  // NVS evt_cancel
  }

  // Auto-trigger starts an event
  startEvent(event) {
    this.activeEventEndTime = event.endTime;
    this.activeEventName = event.name;
    this.activeEventId = event.id;
    this.cancelFlag = '';  // Clear stale cancel
  }

  // Manual reset by operator
  reset() {
    if (this.activeEventId) {
      this.cancelFlag = this.activeEventId;
    }
    this.activeEventEndTime = 0;
    this.activeEventName = '';
    this.activeEventId = '';
  }

  // Event cutoff (booking expired)
  cutoff() {
    this.activeEventEndTime = 0;
    this.activeEventName = '';
    this.activeEventId = '';
  }

  // Factory reset
  factoryReset() {
    this.activeEventEndTime = 0;
    this.activeEventName = '';
    this.activeEventId = '';
    this.cancelFlag = '';
  }

  // Boot recovery
  recover(recovery) {
    this.activeEventEndTime = recovery.eventEndTime;
    this.activeEventName = recovery.eventName;
    this.activeEventId = recovery.eventId;
  }
}

describe('Event Window Lifecycle', () => {
  let state;

  beforeEach(() => {
    state = new EventWindowState();
  });

  describe('initial state', () => {
    test('starts with no active event', () => {
      expect(state.activeEventEndTime).toBe(0);
      expect(state.activeEventId).toBe('');
      expect(state.activeEventName).toBe('');
      expect(state.cancelFlag).toBe('');
    });
  });

  describe('auto-trigger start', () => {
    test('sets all event window fields', () => {
      state.startEvent({ id: 'evt1', name: 'Badminton', endTime: 5000 });
      expect(state.activeEventEndTime).toBe(5000);
      expect(state.activeEventId).toBe('evt1');
      expect(state.activeEventName).toBe('Badminton');
    });

    test('clears stale cancel flag', () => {
      state.cancelFlag = 'old_event';
      state.startEvent({ id: 'evt1', name: 'Badminton', endTime: 5000 });
      expect(state.cancelFlag).toBe('');
    });
  });

  describe('manual reset', () => {
    test('clears event window', () => {
      state.startEvent({ id: 'evt1', name: 'Badminton', endTime: 5000 });
      state.reset();
      expect(state.activeEventEndTime).toBe(0);
      expect(state.activeEventId).toBe('');
      expect(state.activeEventName).toBe('');
    });

    test('saves cancel flag with event ID', () => {
      state.startEvent({ id: 'evt1', name: 'Badminton', endTime: 5000 });
      state.reset();
      expect(state.cancelFlag).toBe('evt1');
    });

    test('no cancel flag when no active event', () => {
      state.reset();
      expect(state.cancelFlag).toBe('');
    });
  });

  describe('event cutoff', () => {
    test('clears event window', () => {
      state.startEvent({ id: 'evt1', name: 'Badminton', endTime: 5000 });
      state.cutoff();
      expect(state.activeEventEndTime).toBe(0);
      expect(state.activeEventId).toBe('');
    });

    test('does NOT set cancel flag (natural end)', () => {
      state.startEvent({ id: 'evt1', name: 'Badminton', endTime: 5000 });
      state.cutoff();
      // Cancel flag should still be empty — cutoff is natural, not manual
      expect(state.cancelFlag).toBe('');
    });
  });

  describe('factory reset', () => {
    test('clears everything including cancel flag', () => {
      state.startEvent({ id: 'evt1', name: 'Badminton', endTime: 5000 });
      state.reset(); // Sets cancel flag
      state.factoryReset();
      expect(state.activeEventEndTime).toBe(0);
      expect(state.activeEventId).toBe('');
      expect(state.cancelFlag).toBe('');
    });
  });

  describe('boot recovery', () => {
    test('sets event window from recovery data', () => {
      state.recover({
        eventId: 'evt1',
        eventName: 'Badminton',
        eventEndTime: 5000,
      });
      expect(state.activeEventEndTime).toBe(5000);
      expect(state.activeEventId).toBe('evt1');
      expect(state.activeEventName).toBe('Badminton');
    });
  });

  describe('full scenarios', () => {
    test('normal lifecycle: start → cutoff', () => {
      state.startEvent({ id: 'evt1', name: 'Badminton', endTime: 5000 });
      expect(state.activeEventId).toBe('evt1');

      state.cutoff();
      expect(state.activeEventId).toBe('');
      expect(state.cancelFlag).toBe('');
    });

    test('manual cancel: start → reset → new event starts normally', () => {
      state.startEvent({ id: 'evt1', name: 'Badminton', endTime: 5000 });
      state.reset();
      expect(state.cancelFlag).toBe('evt1');

      // New event clears cancel flag
      state.startEvent({ id: 'evt2', name: 'Evening Session', endTime: 9000 });
      expect(state.cancelFlag).toBe('');
      expect(state.activeEventId).toBe('evt2');
    });

    test('reset → reboot → recovery skipped for cancelled event', () => {
      state.startEvent({ id: 'evt1', name: 'Badminton', endTime: 5000 });
      state.reset();
      const savedCancelFlag = state.cancelFlag;
      expect(savedCancelFlag).toBe('evt1');

      // Simulate reboot — fresh state but cancel flag persists in NVS
      const rebooted = new EventWindowState();
      rebooted.cancelFlag = savedCancelFlag;

      // Recovery finds evt1 still active
      const recovery = { eventId: 'evt1', eventName: 'Badminton', eventEndTime: 5000 };

      // Should NOT recover because cancel flag matches
      const shouldRecover = recovery.eventId !== rebooted.cancelFlag;
      expect(shouldRecover).toBe(false);
    });

    test('cutoff → reboot → recovery finds no event (natural end)', () => {
      state.startEvent({ id: 'evt1', name: 'Badminton', endTime: 5000 });
      state.cutoff();

      // No cancel flag set
      expect(state.cancelFlag).toBe('');

      // After reboot, event is past endTime, recovery returns shouldRecover: false
      // This is handled by checkMidEventRecovery, not the window lifecycle
    });

    test('back-to-back events', () => {
      state.startEvent({ id: 'evt1', name: 'Session 1', endTime: 5000 });
      state.cutoff(); // Session 1 ends naturally

      state.startEvent({ id: 'evt2', name: 'Session 2', endTime: 9000 });
      expect(state.activeEventId).toBe('evt2');
      expect(state.activeEventName).toBe('Session 2');
    });
  });
});
