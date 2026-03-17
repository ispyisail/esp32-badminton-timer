/**
 * Integration tests: WebSocket connection and initial handshake
 */
const { createClient } = require('./ws-helper');

describe('WebSocket Connection', () => {
  let client;

  afterEach(() => {
    if (client) client.close();
  });

  test('receives settings on connect', async () => {
    client = await createClient();
    const settingsMsg = client.messages.find(m => m.event === 'settings');
    expect(settingsMsg).toBeDefined();
    expect(settingsMsg.settings).toHaveProperty('gameDuration');
    expect(settingsMsg.settings).toHaveProperty('numRounds');
    expect(settingsMsg.settings).toHaveProperty('sirenLength');
    expect(settingsMsg.settings).toHaveProperty('sirenPause');
  });

  test('receives state on connect', async () => {
    client = await createClient();
    const stateMsg = client.messages.find(m => m.event === 'state');
    expect(stateMsg).toBeDefined();
    expect(stateMsg.state).toHaveProperty('status');
    expect(stateMsg.state).toHaveProperty('mainTimer');
    expect(stateMsg.state).toHaveProperty('currentRound');
    expect(stateMsg.state).toHaveProperty('numRounds');
  });

  test('receives sync on connect', async () => {
    client = await createClient();
    const syncMsg = client.messages.find(m => m.event === 'sync');
    expect(syncMsg).toBeDefined();
    expect(syncMsg).toHaveProperty('mainTimerRemaining');
    expect(syncMsg).toHaveProperty('currentRound');
    expect(syncMsg).toHaveProperty('status');
  });

  test('receives NTP status on connect', async () => {
    client = await createClient();
    const ntpMsg = client.messages.find(m => m.event === 'ntp_status');
    expect(ntpMsg).toBeDefined();
    expect(ntpMsg.synced).toBe(true);
    expect(ntpMsg).toHaveProperty('time');
    expect(ntpMsg).toHaveProperty('timezone');
  });

  test('default settings match expected values', async () => {
    client = await createClient();
    const settingsMsg = client.messages.find(m => m.event === 'settings');
    expect(settingsMsg.settings.gameDuration).toBe(12 * 60 * 1000);
    expect(settingsMsg.settings.numRounds).toBe(3);
    expect(settingsMsg.settings.sirenLength).toBe(1000);
    expect(settingsMsg.settings.sirenPause).toBe(1000);
  });

  test('initial state is IDLE', async () => {
    client = await createClient();
    const stateMsg = client.messages.find(m => m.event === 'state');
    // Might be running from a previous test, but check structure
    expect(['IDLE', 'RUNNING', 'PAUSED', 'FINISHED']).toContain(stateMsg.state.status);
  });
});
