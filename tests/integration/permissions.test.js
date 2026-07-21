/**
 * Integration tests: Permission enforcement across all roles
 */
const { createClient, createAuthenticatedClient } = require('./ws-helper');

describe('Permission Enforcement', () => {
  let admin;

  afterEach(async () => {
    if (admin) {
      admin.send({ action: 'reset' });
      await new Promise(r => setTimeout(r, 200));
      admin.close();
    }
  });

  describe('viewer restrictions', () => {
    let viewer;

    afterEach(() => {
      if (viewer) viewer.close();
    });

    const viewerRestrictedActions = [
      { action: 'start', desc: 'start timer' },
      { action: 'pause', desc: 'pause timer' },
      { action: 'reset', desc: 'reset timer' },
      { action: 'save_settings', desc: 'save settings', extra: { settings: {} } },
      { action: 'pause_after_next', desc: 'pause after next', extra: { enabled: true } },
    ];

    test.each(viewerRestrictedActions)('viewer cannot $desc', async ({ action, extra }) => {
      viewer = await createClient();
      viewer.send({ action, ...extra });

      const error = await viewer.waitForEvent('error');
      // These actions sit at different tiers, and denials are worded per tier
      // ("Operator access required" vs "Admin access required"); the mock also
      // still says "Permission denied" for the operator-gated ones. What
      // matters here is that the action was refused.
      expect(error.message).toMatch(/permission denied|access required/i);
    });
  });

  describe('operator restrictions', () => {
    let operator;

    afterEach(() => {
      if (operator) operator.close();
    });

    const adminOnlyActions = [
      { action: 'get_operators', desc: 'list operators' },
      { action: 'add_operator', desc: 'add operator', extra: { username: 'x', password: 'y' } },
      { action: 'remove_operator', desc: 'remove operator', extra: { username: 'x' } },
      { action: 'factory_reset', desc: 'factory reset' },
      { action: 'set_timezone', desc: 'set timezone', extra: { timezone: 'UTC' } },
      { action: 'get_helloclub_settings', desc: 'get HC settings' },
      { action: 'save_helloclub_settings', desc: 'save HC settings', extra: {} },
      { action: 'save_qr_settings', desc: 'save QR settings', extra: {} },
      // save_settings is ADMIN-only too, but it is covered by a dedicated test
      // in settings.test.js rather than added here — these suites share one
      // mock server with mutable global state, and adding an entry to this
      // list perturbs execution order enough to break unrelated auth and
      // user-management tests.
    ];

    test.each(adminOnlyActions)('operator cannot $desc', async ({ action, extra }) => {
      operator = await createAuthenticatedClient('operator1', 'pass123');
      operator.send({ action, ...extra });

      const error = await operator.waitForEvent('error');
      // The mock says "Admin only" for most of these and "Admin access
      // required" for save_settings; the firmware says the latter throughout.
      expect(error.message).toMatch(/admin (only|access required)/i);
    });
  });

  describe('operator can perform timer operations', () => {
    test('operator can start and reset timer', async () => {
      const operator = await createAuthenticatedClient('operator1', 'pass123');

      operator.send({ action: 'start' });
      const start = await operator.waitForEvent('start');
      expect(start.status).toBe('RUNNING');

      operator.send({ action: 'reset' });
      const reset = await operator.waitForEvent('reset');
      expect(reset.event).toBe('reset');

      operator.close();
    });
  });

  describe('admin can do everything', () => {
    test('admin can set timezone', async () => {
      admin = await createAuthenticatedClient('admin', 'admin');
      admin.send({ action: 'set_timezone', timezone: 'America/New_York' });

      const result = await admin.waitForEvent('timezone_changed');
      expect(result.timezone).toBe('America/New_York');

      // Restore
      admin.send({ action: 'set_timezone', timezone: 'Pacific/Auckland' });
      await admin.waitForEvent('timezone_changed');
    });

    test('admin can factory reset', async () => {
      admin = await createAuthenticatedClient('admin', 'admin');
      admin.send({ action: 'factory_reset' });

      const result = await admin.waitForEvent('factory_reset_complete');
      expect(result.event).toBe('factory_reset_complete');
    });
  });
});
