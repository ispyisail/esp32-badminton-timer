/**
 * Registered as `setupFilesAfterEnv` for the integration project, so this runs
 * once per test file — giving every file its own server on its own port, with
 * no state carried in from any other file.
 */
const { startTestServer } = require('./server-harness');

let server;

beforeAll(async () => {
  server = await startTestServer();
  // ws-helper reads this lazily, per connection, so it always sees this file's port.
  process.env.TEST_SERVER_PORT = String(server.port);
}, 30000);

afterAll(async () => {
  if (server) {
    await server.stop();
    server = undefined;
  }
});
