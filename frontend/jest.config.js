export default {
  projects: [
    {
      // Frontend logic: utils, hooks, preferences (browser-like environment).
      displayName: 'unit',
      testEnvironment: 'jsdom',
      testMatch: ['<rootDir>/src/**/*.test.{js,jsx}'],
      moduleNameMapper: { '\\.css$': '<rootDir>/tests/styleStub.js' },
      setupFilesAfterEnv: ['<rootDir>/tests/setup.js'],    },
    {
      // API contract: runs against local Flask, or any backend via API_URL.
      displayName: 'contract',
      testEnvironment: 'node',
      testMatch: ['<rootDir>/tests/contract/**/*.test.js'],
    },
  ],
}
