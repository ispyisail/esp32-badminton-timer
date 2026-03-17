/**
 * Integration tests: QR code configuration
 */
const { createClient, createAuthenticatedClient } = require('./ws-helper');

describe('QR Config', () => {
  let admin;

  afterEach(async () => {
    if (admin) admin.close();
  });

  test('any user can get QR config', async () => {
    const viewer = await createClient();
    viewer.send({ action: 'get_qr_config' });

    const result = await viewer.waitForEvent('qr_config');
    expect(result).toHaveProperty('ssid');
    expect(result).toHaveProperty('password');
    expect(result).toHaveProperty('encryption');
    expect(result).toHaveProperty('appUrl');
    viewer.close();
  });

  test('admin can save QR settings', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({
      action: 'save_qr_settings',
      ssid: 'TestWiFi',
      password: 'testpass',
      encryption: 'WPA',
    });

    const result = await admin.waitForEvent('qr_settings_saved');
    expect(result.event).toBe('qr_settings_saved');

    // Verify saved
    admin.clearMessages();
    admin.send({ action: 'get_qr_config' });
    const config = await admin.waitForEvent('qr_config');
    expect(config.ssid).toBe('TestWiFi');
    expect(config.password).toBe('testpass');
  });

  test('viewer cannot save QR settings', async () => {
    const viewer = await createClient();
    viewer.send({ action: 'save_qr_settings', ssid: 'Hack', password: 'x' });

    const error = await viewer.waitForEvent('error');
    expect(error.message).toContain('Admin only');
    viewer.close();
  });
});
