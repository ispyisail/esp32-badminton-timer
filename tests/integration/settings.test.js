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
    expect(error.message).toMatch(/admin access required/i);
  });

  // save_settings is ADMIN-only. Every other source agrees: main.cpp lists it
  // under needsAdmin, the mock server checks role !== 'admin', API.md says
  // "Permission: ADMIN only", and every settings card in index.html carries
  // .admin-only. This test previously asserted the opposite and had been
  // failing against both implementations.
  test('operator cannot save settings', async () => {
    const operator = await createAuthenticatedClient('operator1', 'pass123');
    operator.send({
      action: 'save_settings',
      settings: { gameDuration: 600000, numRounds: 4, sirenLength: 1000, sirenPause: 1000 }
    });

    const error = await operator.waitForEvent('error');
    expect(error.message).toMatch(/admin access required/i);
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
