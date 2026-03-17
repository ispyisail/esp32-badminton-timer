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
      globalSetup: path.join(__dirname, 'integration/global-setup.js'),
      globalTeardown: path.join(__dirname, 'integration/global-teardown.js'),
    },
  ],
};
