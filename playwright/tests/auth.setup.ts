/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/
import { test as setup, expect } from '@playwright/test';

const authFile = '.auth/client.json';

/**
 * @Description Save cookies (required for the entire testsuite).
 * Get and maintains  cookie to preserve the 'Controller' lock.
 */
setup('Get client id', async ({ page }) => {
  
  // Navigate to the app to trigger the cookie creation
  await page.goto('/');
  
  // Poll until the specific auth cookie appears
  await expect(async () => {
    const cookies = await page.context().cookies();
    const hasAuth = cookies.some(c => c.name === 'client_id');
    if (!hasAuth) throw new Error('Client id not setting up');
  }).toPass({ timeout: 2000, intervals: [1000] });

  // Save the storage state (cookies) to a file
  await page.context().storageState({ path: authFile });
});
