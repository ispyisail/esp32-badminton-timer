/**
 * Integration tests: Timer control (start, pause, resume, reset)
 */
const { createClient, createAuthenticatedClient } = require('./ws-helper');

describe('Timer Control', () => {
  let admin;
  let viewer;

  afterEach(async () => {
    // Reset timer to clean state
    if (admin) {
      admin.send({ action: 'reset' });
      await new Promise(r => setTimeout(r, 200));
      admin.close();
    }
    if (viewer) viewer.close();
  });

  test('operator can start timer', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'start' });

    const startMsg = await admin.waitForEvent('start');
    expect(startMsg.status).toBe('RUNNING');
    expect(startMsg.gameDuration).toBe(12 * 60 * 1000);
    expect(startMsg.currentRound).toBe(1);
    expect(startMsg.numRounds).toBe(3);
  });

  test('viewer cannot start timer', async () => {
    viewer = await createClient();
    viewer.send({ action: 'start' });

    const error = await viewer.waitForEvent('error');
    expect(error.message).toContain('Permission denied');
  });

  test('admin can pause running timer', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'start' });
    await admin.waitForEvent('start');

    admin.clearMessages();
    admin.send({ action: 'pause' });

    const pauseMsg = await admin.waitForEvent('pause');
    expect(pauseMsg.event).toBe('pause');
    expect(pauseMsg.mainTimerRemaining).toBeGreaterThan(0);
  });

  test('admin can resume paused timer', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'start' });
    await admin.waitForEvent('start');

    admin.send({ action: 'pause' });
    await admin.waitForEvent('pause');

    admin.clearMessages();
    admin.send({ action: 'pause' }); // Toggle to resume

    const resumeMsg = await admin.waitForEvent('resume');
    expect(resumeMsg.event).toBe('resume');
  });

  test('admin can reset timer', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'start' });
    await admin.waitForEvent('start');

    admin.clearMessages();
    admin.send({ action: 'reset' });

    const resetMsg = await admin.waitForEvent('reset');
    expect(resetMsg.event).toBe('reset');
  });

  test('viewer cannot pause timer', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'start' });
    await admin.waitForEvent('start');

    viewer = await createClient();
    viewer.send({ action: 'pause' });

    const error = await viewer.waitForEvent('error');
    expect(error.message).toContain('Permission denied');
  });

  test('viewer cannot reset timer', async () => {
    viewer = await createClient();
    viewer.send({ action: 'reset' });

    const error = await viewer.waitForEvent('error');
    expect(error.message).toContain('Permission denied');
  });

  test('starting sends correct round info', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'start' });

    const startMsg = await admin.waitForEvent('start');
    expect(startMsg.currentRound).toBe(1);
    expect(startMsg.numRounds).toBe(3);
    expect(startMsg.pauseAfterNext).toBe(false);
    expect(startMsg.continuousMode).toBe(false);
  });
});
