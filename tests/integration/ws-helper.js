const WebSocket = require('ws');

const PORT = process.env.TEST_SERVER_PORT || 18080;
const WS_URL = `ws://localhost:${PORT}/ws`;

/**
 * Create a WebSocket connection and collect initial messages.
 * Returns { ws, messages, waitForEvent, send, close }
 */
function createClient() {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(WS_URL);
    const messages = [];
    const waiters = [];

    ws.on('message', (data) => {
      const msg = JSON.parse(data.toString());
      messages.push(msg);

      // Check if any waiters match
      for (let i = waiters.length - 1; i >= 0; i--) {
        const { eventType, predicate, resolve: res, timer } = waiters[i];
        if (msg.event === eventType && (!predicate || predicate(msg))) {
          clearTimeout(timer);
          waiters.splice(i, 1);
          res(msg);
        }
      }
    });

    ws.on('open', () => {
      // Give initial messages time to arrive
      setTimeout(() => {
        resolve({
          ws,
          messages,

          /** Wait for a specific event type. Optional predicate to filter. */
          waitForEvent(eventType, predicate = null, timeoutMs = 5000) {
            // Check already-received messages
            const existing = messages.find(
              m => m.event === eventType && (!predicate || predicate(m))
            );
            if (existing) return Promise.resolve(existing);

            return new Promise((res, rej) => {
              const timer = setTimeout(
                () => rej(new Error(`Timeout waiting for event: ${eventType}`)),
                timeoutMs
              );
              waiters.push({ eventType, predicate, resolve: res, timer });
            });
          },

          /** Send a JSON message */
          send(obj) {
            ws.send(JSON.stringify(obj));
          },

          /** Authenticate as a specific role */
          async authenticate(username = '', password = '') {
            ws.send(JSON.stringify({ action: 'authenticate', username, password }));
            return new Promise((res, rej) => {
              const timer = setTimeout(
                () => rej(new Error('Auth timeout')),
                5000
              );
              waiters.push({
                eventType: 'auth_success',
                predicate: null,
                resolve: (msg) => { clearTimeout(timer); res(msg); },
                timer,
              });
              // Also handle auth_failed
              waiters.push({
                eventType: 'auth_failed',
                predicate: null,
                resolve: (msg) => { clearTimeout(timer); res(msg); },
                timer: setTimeout(() => {}, 5000),
              });
            });
          },

          /** Clear collected messages */
          clearMessages() {
            messages.length = 0;
          },

          /** Close the connection */
          close() {
            ws.close();
          },
        });
      }, 200);
    });

    ws.on('error', reject);
  });
}

/**
 * Create an authenticated client.
 */
async function createAuthenticatedClient(username, password) {
  const client = await createClient();
  await client.authenticate(username, password);
  client.clearMessages();
  return client;
}

module.exports = { createClient, createAuthenticatedClient, WS_URL, PORT };
