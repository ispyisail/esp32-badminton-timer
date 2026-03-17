const { spawn } = require('child_process');
const path = require('path');

module.exports = async function () {
  const serverDir = path.join(__dirname, '..', '..', 'test-server');

  // Start the test server on a random-ish port to avoid conflicts
  const port = 18080;
  process.env.TEST_SERVER_PORT = String(port);

  const serverProcess = spawn('node', ['-e', `
    process.env.PORT = '${port}';
    // Patch the server to use our port
    const origListen = require('http').Server.prototype.listen;
    require('http').Server.prototype.listen = function(p, ...args) {
      return origListen.call(this, ${port}, ...args);
    };
    require('./server.js');
  `], {
    cwd: serverDir,
    stdio: ['pipe', 'pipe', 'pipe'],
    env: { ...process.env, PORT: String(port) },
  });

  // Store PID for teardown
  globalThis.__TEST_SERVER_PID__ = serverProcess.pid;
  process.env.TEST_SERVER_PID = String(serverProcess.pid);

  // Wait for server to be ready
  const WebSocket = require('ws');
  const maxWait = 10000;
  const start = Date.now();

  while (Date.now() - start < maxWait) {
    try {
      await new Promise((resolve, reject) => {
        const ws = new WebSocket(`ws://localhost:${port}/ws`);
        ws.on('open', () => { ws.close(); resolve(); });
        ws.on('error', reject);
        setTimeout(() => reject(new Error('timeout')), 1000);
      });
      console.log(`\nTest server started on port ${port} (PID ${serverProcess.pid})`);
      return;
    } catch {
      await new Promise(r => setTimeout(r, 300));
    }
  }

  serverProcess.kill();
  throw new Error('Test server failed to start within 10 seconds');
};
