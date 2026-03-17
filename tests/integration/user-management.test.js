/**
 * Integration tests: User management (operators, passwords)
 */
const { createClient, createAuthenticatedClient } = require('./ws-helper');

describe('User Management', () => {
  let admin;

  afterEach(async () => {
    if (admin) admin.close();
  });

  test('admin can list operators', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'get_operators' });

    const result = await admin.waitForEvent('operators_list');
    expect(result.operators).toBeInstanceOf(Array);
    expect(result.operators).toContain('operator1');
    expect(result.operators).toContain('operator2');
  });

  test('viewer cannot list operators', async () => {
    const viewer = await createClient();
    viewer.send({ action: 'get_operators' });

    const error = await viewer.waitForEvent('error');
    expect(error.message).toContain('Admin only');
    viewer.close();
  });

  test('admin can add operator', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'add_operator', username: 'testop', password: 'testpass' });

    const result = await admin.waitForEvent('operator_added');
    expect(result.username).toBe('testop');

    // Verify new operator can login
    const newOp = await createClient();
    const authResult = await newOp.authenticate('testop', 'testpass');
    expect(authResult.event).toBe('auth_success');
    expect(authResult.role).toBe('operator');
    newOp.close();

    // Clean up
    admin.send({ action: 'remove_operator', username: 'testop' });
    await admin.waitForEvent('operator_removed');
  });

  test('admin can remove operator', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');

    // Add then remove
    admin.send({ action: 'add_operator', username: 'tempop', password: 'temppass' });
    await admin.waitForEvent('operator_added');

    admin.clearMessages();
    admin.send({ action: 'remove_operator', username: 'tempop' });

    const result = await admin.waitForEvent('operator_removed');
    expect(result.username).toBe('tempop');

    // Verify removed operator can't login
    const removed = await createClient();
    removed.send({ action: 'authenticate', username: 'tempop', password: 'temppass' });
    const authResult = await removed.waitForEvent('auth_failed');
    expect(authResult.event).toBe('auth_failed');
    removed.close();
  });

  test('operator cannot add operators', async () => {
    const operator = await createAuthenticatedClient('operator1', 'pass123');
    operator.send({ action: 'add_operator', username: 'hack', password: 'hack' });

    const error = await operator.waitForEvent('error');
    expect(error.message).toContain('Admin only');
    operator.close();
  });

  test('admin can change password', async () => {
    admin = await createAuthenticatedClient('admin', 'admin');
    admin.send({ action: 'change_password', newPassword: 'newpass' });

    const result = await admin.waitForEvent('password_changed');
    expect(result.event).toBe('password_changed');
  });

  test('operator can change own password with correct old password', async () => {
    const operator = await createAuthenticatedClient('operator1', 'pass123');
    operator.send({ action: 'change_password', oldPassword: 'pass123', newPassword: 'newpass123' });

    const result = await operator.waitForEvent('password_changed');
    expect(result.event).toBe('password_changed');

    // Restore original password
    operator.send({ action: 'change_password', oldPassword: 'newpass123', newPassword: 'pass123' });
    await operator.waitForEvent('password_changed');
    operator.close();
  });

  test('operator cannot change password with wrong old password', async () => {
    const operator = await createAuthenticatedClient('operator1', 'pass123');
    operator.send({ action: 'change_password', oldPassword: 'wrong', newPassword: 'newpass' });

    const error = await operator.waitForEvent('error');
    expect(error.message).toContain('incorrect');
    operator.close();
  });
});
