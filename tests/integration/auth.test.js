/**
 * Integration tests: Authentication flows
 */
const { createClient } = require('./ws-helper');

describe('Authentication', () => {
  let client;

  afterEach(() => {
    if (client) client.close();
  });

  test('admin login succeeds with correct credentials', async () => {
    client = await createClient();
    const result = await client.authenticate('admin', 'admin');
    expect(result.event).toBe('auth_success');
    expect(result.role).toBe('admin');
    expect(result.username).toBe('admin');
  });

  test('operator login succeeds', async () => {
    client = await createClient();
    const result = await client.authenticate('operator1', 'pass123');
    expect(result.event).toBe('auth_success');
    expect(result.role).toBe('operator');
    expect(result.username).toBe('operator1');
  });

  test('viewer mode with empty credentials', async () => {
    client = await createClient();
    client.send({ action: 'authenticate', username: '', password: '' });
    const result = await client.waitForEvent('auth_success');
    expect(result.role).toBe('viewer');
  });

  test('invalid credentials are rejected', async () => {
    client = await createClient();
    client.send({ action: 'authenticate', username: 'admin', password: 'wrong' });
    const result = await client.waitForEvent('auth_failed');
    expect(result.event).toBe('auth_failed');
    expect(result.message).toContain('Invalid');
  });

  test('unknown username is rejected', async () => {
    client = await createClient();
    client.send({ action: 'authenticate', username: 'nobody', password: 'test' });
    const result = await client.waitForEvent('auth_failed');
    expect(result.event).toBe('auth_failed');
  });
});
