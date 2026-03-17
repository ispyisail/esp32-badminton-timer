/**
 * Integration tests: Broadcast — all clients receive state updates
 */
const { createClient, createAuthenticatedClient } = require('./ws-helper');

describe('Broadcast', () => {
  let admin;
  let viewer1;
  let viewer2;

  afterEach(async () => {
    if (admin) {
      admin.send({ action: 'reset' });
      await new Promise(r => setTimeout(r, 200));
      admin.close();
    }
    if (viewer1) viewer1.close();
    if (viewer2) viewer2.close();
  });

  test('all clients receive start event', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    viewer1 = await createClient();
    viewer2 = await createClient();

    viewer1.clearMessages();
    viewer2.clearMessages();

    admin.send({ action: 'start' });

    const [v1Start, v2Start] = await Promise.all([
      viewer1.waitForEvent('start'),
      viewer2.waitForEvent('start'),
    ]);

    expect(v1Start.status).toBe('RUNNING');
    expect(v2Start.status).toBe('RUNNING');
  });

  test('all clients receive pause event', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    viewer1 = await createClient();

    admin.send({ action: 'start' });
    await Promise.all([
      admin.waitForEvent('start'),
      viewer1.waitForEvent('start'),
    ]);

    viewer1.clearMessages();
    admin.send({ action: 'pause' });

    const v1Pause = await viewer1.waitForEvent('pause');
    expect(v1Pause.event).toBe('pause');
  });

  test('all clients receive reset event', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    viewer1 = await createClient();

    admin.send({ action: 'start' });
    await viewer1.waitForEvent('start');

    viewer1.clearMessages();
    admin.send({ action: 'reset' });

    const v1Reset = await viewer1.waitForEvent('reset');
    expect(v1Reset.event).toBe('reset');
  });

  test('all clients receive settings broadcast', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    viewer1 = await createClient();

    viewer1.clearMessages();
    admin.send({
      action: 'save_settings',
      settings: { gameDuration: 600000, numRounds: 5, sirenLength: 2000, sirenPause: 1500 }
    });

    const v1Settings = await viewer1.waitForEvent('settings');
    expect(v1Settings.settings.gameDuration).toBe(600000);
    expect(v1Settings.settings.numRounds).toBe(5);

    // Restore defaults
    admin.send({
      action: 'save_settings',
      settings: { gameDuration: 720000, numRounds: 3, sirenLength: 1000, sirenPause: 1000 }
    });
  });
});
