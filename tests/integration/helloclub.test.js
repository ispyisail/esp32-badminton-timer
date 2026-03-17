/**
 * Integration tests: Hello Club integration
 */
const { createAuthenticatedClient } = require('./ws-helper');

describe('Hello Club Integration', () => {
  let admin;

  afterEach(async () => {
    if (admin) admin.close();
  });

  test('admin can get HC settings', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'get_helloclub_settings' });

    const result = await admin.waitForEvent('helloclub_settings');
    expect(result).toHaveProperty('apiKey');
    expect(result).toHaveProperty('enabled');
    expect(result).toHaveProperty('defaultDuration');
  });

  test('admin can save HC settings', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({
      action: 'save_helloclub_settings',
      enabled: false,
      defaultDuration: 15,
    });

    const result = await admin.waitForEvent('helloclub_settings_saved');
    expect(result.event).toBe('helloclub_settings_saved');
  });

  test('any user can get upcoming events', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'get_upcoming_events' });

    const result = await admin.waitForEvent('upcoming_events');
    expect(result).toHaveProperty('events');
    expect(result.events).toBeInstanceOf(Array);
  });

  test('refresh without API key returns error', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');

    // Clear API key first
    admin.send({ action: 'save_helloclub_settings', apiKey: '', enabled: false });
    await admin.waitForEvent('helloclub_settings_saved');

    admin.clearMessages();
    admin.send({ action: 'helloclub_refresh' });

    const result = await admin.waitForEvent('helloclub_refresh_result');
    expect(result.success).toBe(false);
    expect(result.message).toContain('API key');
  });
});
