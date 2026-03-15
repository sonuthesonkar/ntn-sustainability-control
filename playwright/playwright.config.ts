import { defineConfig, devices } from '@playwright/test';

export default defineConfig({
  testDir: './tests',
  
  /* Run tests in files in parallel */
  fullyParallel: true,
  
  /* Set concurrent browsers) */
  workers: 1, // Run with single browser (required due to cookie based controller and observer logic)

  /* Reporter to use. See https://playwright.dev */
  reporter: [
    ['github'],
    ['list', { printSteps: true }],               // Detailed terminal output
    ['html', { open: 'never', outputFolder: 'playwright-report' }], // Interactive HTML report
  ],
  use: {
    // Force the "full" browser engine instead of the lightweight shell
    channel: 'chromium',
    headless: true,
    
    // Base URL to use in actions like `await page.goto('/')`.
    baseURL: 'http://localhost:3000',

    // Collect trace when retrying the failed test. See https://playwright.dev
    trace: 'on-first-retry',
    
    // Capture screenshot after each test failure.
    screenshot: 'only-on-failure',
    
    // Record video only on failure.
    video: 'retain-on-failure',
  },

  /* Configure projects for major browsers */
  projects: [
    // Setup project
    {
      name: 'Setup',
      testMatch: /.*\.setup\.ts/,
    },
    // Main project
    {
      name: 'Test',
      use: { 
        ...devices['Desktop Chrome'],
        // Tell Playwright to load the cookie from the saved file
        storageState: '.auth/client.json', 
      },
      dependencies: ['Setup'], // Ensures setup runs before tests
    },
  ],
});
