/**
 * Integration tests: Timer sync broadcasts while running
 */
const { createClient, createAuthenticatedClient } = require('./ws-helper');

describe('Timer Sync', () => {
  let admin;

  afterEach(async () => {
    if (admin) {
      admin.send({ action: 'reset' });
      await new Promise(r => setTimeout(r, 500));
      admin.close();
    }
  });

  test('receives periodic sync while timer is running', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'start' });
    await admin.waitForEvent('start');

    admin.clearMessages();

    // Wait for a sync broadcast (server ticks every 1s when running)
    const sync = await admin.waitForEvent('sync', null, 3000);
    expect(sync.event).toBe('sync');
    expect(sync.status).toBe('RUNNING');
    expect(sync.mainTimerRemaining).toBeGreaterThanOrEqual(0);
    expect(sync.currentRound).toBe(1);
    expect(sync).toHaveProperty('numRounds');
    expect(sync).toHaveProperty('pauseAfterNext');
    expect(sync).toHaveProperty('continuousMode');
  });

  test('sync includes pause-after-next flag', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'start' });
    await admin.waitForEvent('start');

    admin.send({ action: 'pause_after_next', enabled: true });

    // Wait for sync that reflects the flag
    const sync = await admin.waitForEvent('sync', m => m.pauseAfterNext === true, 3000);
    expect(sync.pauseAfterNext).toBe(true);
  });

  test('new client connecting mid-game receives current state', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');

    // Ensure timer is started and wait for server to tick
    admin.send({ action: 'start' });
    await admin.waitForEvent('start');

    // Wait for at least 2 server ticks (1s each) so remaining is populated
    const sync = await admin.waitForEvent('sync',
      m => m.status === 'RUNNING' && m.mainTimerRemaining > 0, 5000);
    expect(sync.mainTimerRemaining).toBeGreaterThan(0);

    // Now connect a late client — timer is confirmed running
    const lateClient = await createClient();

    // The test server sends state + sync on connect. Wait briefly for all messages.
    await new Promise(r => setTimeout(r, 300));

    // Check that the late client received messages about an active timer
    const syncMsgs = lateClient.messages.filter(m => m.event === 'sync');
    const stateMsgs = lateClient.messages.filter(m => m.event === 'state');

    expect(stateMsgs.length).toBeGreaterThan(0);
    expect(syncMsgs.length).toBeGreaterThan(0);

    // The sync should have remaining time (it's built from the live server state)
    const latestSync = syncMsgs[syncMsgs.length - 1];
    expect(latestSync.mainTimerRemaining).toBeGreaterThan(0);

    lateClient.close();
  });

});
