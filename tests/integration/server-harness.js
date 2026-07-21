/**
 * Starts an isolated mock-server process per test file.
 *
 * The suites previously shared one server on a fixed port. That server holds
 * global mutable state (timer, settings, operators), so whichever suite ran
 * first decided what the others saw — results varied between runs of identical
 * code, and `--runInBand` did not help because the leakage is across files, not
 * just across parallel workers.
 */
const { spawn } = require('child_process');
const path = require('path');

const SERVER_DIR = path.join(__dirname, '..', '..', 'test-server');
const PORT_LINE = /TEST_SERVER_PORT=(\d+)/;

/**
 * Spawn a server on an OS-assigned port and resolve once it reports that port.
 * @returns {Promise<{ port: number, stop: () => Promise<void> }>}
 */
function startTestServer({ timeoutMs = 20000 } = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(process.execPath, ['server.js'], {
      cwd: SERVER_DIR,
      env: { ...process.env, PORT: '0' },   // 0 = let the OS pick, so workers never collide
      stdio: ['ignore', 'pipe', 'pipe'],
    });

    let output = '';
    let settled = false;

    const finish = (fn, arg) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      fn(arg);
    };

    const timer = setTimeout(() => {
      child.kill();
      finish(reject, new Error(
        `Test server did not report a port within ${timeoutMs}ms.\nOutput:\n${output}`
      ));
    }, timeoutMs);

    child.stdout.on('data', (chunk) => {
      output += chunk.toString();
      const match = output.match(PORT_LINE);
      if (match) {
        finish(resolve, { port: Number(match[1]), stop: () => stopServer(child) });
      }
    });

    child.stderr.on('data', (chunk) => { output += chunk.toString(); });

    child.on('error', (err) => finish(reject, err));

    child.on('exit', (code) => finish(reject, new Error(
      `Test server exited before reporting a port (code ${code}).\nOutput:\n${output}`
    )));
  });
}

function stopServer(child) {
  return new Promise((resolve) => {
    if (child.exitCode !== null || child.signalCode !== null) return resolve();

    // Escalate if it ignores the polite request, so a stuck child cannot wedge
    // the run or leak a listening socket into the next file.
    const force = setTimeout(() => {
      try { child.kill('SIGKILL'); } catch { /* already gone */ }
      resolve();
    }, 3000);
    if (typeof force.unref === 'function') force.unref();

    child.once('exit', () => { clearTimeout(force); resolve(); });
    child.kill();
  });
}

module.exports = { startTestServer };
