const path = require('path');

module.exports = {
  testEnvironment: 'node',
  projects: [
    {
      displayName: 'unit',
      testMatch: [path.join(__dirname, 'unit/**/*.test.js')],
      testEnvironment: 'node',
    },
    {
      displayName: 'integration',
      testMatch: [path.join(__dirname, 'integration/**/*.test.js')],
      testEnvironment: 'node',
      // Per-file, not global: each test file gets its own mock-server process
      // on its own port. A single shared server made results depend on which
      // suite ran first, because its timer/settings/operator state is global.
      setupFilesAfterEnv: [path.join(__dirname, 'integration/setup-server.js')],
    },
  ],
};
