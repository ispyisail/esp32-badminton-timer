/**
 * Integration tests: Pause-after-next feature
 */
const { createClient, createAuthenticatedClient } = require('./ws-helper');

describe('Pause After Next', () => {
  let admin;

  afterEach(async () => {
    if (admin) {
      admin.send({ action: 'reset' });
      await new Promise(r => setTimeout(r, 200));
      admin.close();
    }
  });

  test('operator can enable pause-after-next', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'start' });
    await admin.waitForEvent('start');

    admin.clearMessages();
    admin.send({ action: 'pause_after_next', enabled: true });

    const sync = await admin.waitForEvent('sync');
    expect(sync.pauseAfterNext).toBe(true);
  });

  test('operator can disable pause-after-next', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'start' });
    await admin.waitForEvent('start');

    admin.send({ action: 'pause_after_next', enabled: true });
    await admin.waitForEvent('sync', m => m.pauseAfterNext === true);

    admin.clearMessages();
    admin.send({ action: 'pause_after_next', enabled: false });

    const sync = await admin.waitForEvent('sync', m => m.pauseAfterNext === false);
    expect(sync.pauseAfterNext).toBe(false);
  });

  test('viewer cannot set pause-after-next', async () => {
    const viewer = await createClient();
    viewer.send({ action: 'pause_after_next', enabled: true });

    const error = await viewer.waitForEvent('error');
    expect(error.message).toContain('Permission denied');
    viewer.close();
  });
});
