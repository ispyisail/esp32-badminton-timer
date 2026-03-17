/**
 * Integration tests: Settings management
 */
const { createClient, createAuthenticatedClient } = require('./ws-helper');

describe('Settings', () => {
  let admin;
  let viewer;

  afterEach(async () => {
    // Restore default settings
    if (admin) {
      admin.send({
        action: 'save_settings',
        settings: { gameDuration: 720000, numRounds: 3, sirenLength: 1000, sirenPause: 1000 }
      });
      await new Promise(r => setTimeout(r, 200));
      admin.close();
    }
    if (viewer) viewer.close();
  });

  test('admin can save settings', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({
      action: 'save_settings',
      settings: { gameDuration: 900000, numRounds: 5, sirenLength: 2000, sirenPause: 1500 }
    });

    const settingsMsg = await admin.waitForEvent('settings');
    expect(settingsMsg.settings.gameDuration).toBe(900000);
    expect(settingsMsg.settings.numRounds).toBe(5);
    expect(settingsMsg.settings.sirenLength).toBe(2000);
    expect(settingsMsg.settings.sirenPause).toBe(1500);
  });

  test('viewer cannot save settings', async () => {
    viewer = await createClient();
    viewer.send({
      action: 'save_settings',
      settings: { gameDuration: 60000 }
    });

    const error = await viewer.waitForEvent('error');
    expect(error.message).toContain('Permission denied');
  });

  test('operator can save settings', async () => {
    const operator = await createAuthenticatedClient('operator1', 'pass123');
    operator.send({
      action: 'save_settings',
      settings: { gameDuration: 600000, numRounds: 4, sirenLength: 1000, sirenPause: 1000 }
    });

    const settingsMsg = await operator.waitForEvent('settings');
    expect(settingsMsg.settings.gameDuration).toBe(600000);
    expect(settingsMsg.settings.numRounds).toBe(4);
    operator.close();
  });

  test('partial settings update merges with existing', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');

    // Set baseline
    admin.send({
      action: 'save_settings',
      settings: { gameDuration: 720000, numRounds: 3, sirenLength: 1000, sirenPause: 1000 }
    });
    await admin.waitForEvent('settings');

    // Partial update
    admin.clearMessages();
    admin.send({
      action: 'save_settings',
      settings: { gameDuration: 600000 }
    });

    const settingsMsg = await admin.waitForEvent('settings');
    expect(settingsMsg.settings.gameDuration).toBe(600000);
    expect(settingsMsg.settings.numRounds).toBe(3); // Preserved
  });
});
