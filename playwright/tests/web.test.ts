/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/
import { test, expect, type Page } from '@playwright/test';
import * as utils from './utils';

/**
 * Automated NTN Sustainability Control Web GUI Testsuite.
 *
 * Maintains a single browser session to preserve the 'Controller' lock.
 */

/** Expected list of KPIs */
const EXPECTED_KPIS = [
  'Congestion', 'PRB_Util', 'Traffic_Load', 'RAN_Energy', 
  'Carbon_Intensity', 'ISAC_Quality', 'Mobility_Rate'
];

/** Initial KPIs, as returned by server, before the tests start */
let initialKpis: Record<string, number> = {};

/** Main testsuite */
test.describe('Automated NTN Sustainability Control Web GUI Testsuite', () => {
  // 'default' mode runs tests sequentially but does not skip on failure
  test.describe.configure({ mode: 'default' });

  let isObserver = false;

  let sharedPage: Page;
  let h_n_4: number[];  // Latest 4 ntn states

  test.beforeAll(async ({ browser }) => {
    // page.goto happens ONCE. All tests below reuse this open tab.
    sharedPage = await browser.newPage();
    
    await sharedPage.goto('/');
    const mode = await sharedPage.locator('.mode').innerText();
    isObserver = mode === 'Observer';

    // Get history
    const historyData = await utils.getHistoryData(sharedPage);

    // Handle null or error cases
    if (!historyData) throw new Error('Could not find history data');
    if ('error' in historyData) throw new Error(`Error: ${historyData.error}`);

    h_n_4 = historyData.ntn;
  });

  test.afterAll(async () => {
    await sharedPage.close();
  });

  // Verify title & copyright
  test('Verify header title and dynamic copyright', async () => {
    
    await test.step('Check title', async () => {
       // Validate the H1 Title
      const title = sharedPage.getByRole('heading', { name: 'NTN Sustainability Control', level: 1 });
      await expect(title).toBeVisible();   
    });
   
    await test.step('Check dynamic copyright', async () => {
      // Validate the Copyright Statement with the Current Year
      const currentYear = new Date().getFullYear();
      const copyrightText = `(© ${currentYear} Sonu Sonkar)`;
      
      // Locating specifically within the header to avoid duplicates elsewhere
      const copyrightElement = sharedPage.locator('.header .copyright');
      await expect(copyrightElement).toHaveText(copyrightText);
    });
  });

  // Save pre-test KPIs
  test('Save pre-test KPIs', async () => {
    await test.step('Save current KPIs', async () => {
      // Get pre-test KPIs
      const containers = await sharedPage.locator('.slider-container').all();
      for (const container of containers) {
        const name = await container.locator('.caption').first().innerText();
        const value = Number(await container.locator('input[type="range"]').inputValue());
        initialKpis[name] = value;
      }
    });
  });

  // Verify Sliders list
  test('Verify KPIs sliders', async () => {
    const sliders = sharedPage.locator('.slider-container');
    await test.step('Verify slider counts', async () => {
      await expect(sliders).toHaveCount(EXPECTED_KPIS.length);
    });
    
    // Verify each KPI slider
    for (const kpi of EXPECTED_KPIS) {
      await test.step(`Verify KPI name: ${kpi}`, async () => {
        const container = sliders.filter({ hasText: kpi });
        const label = container.locator('.caption').first();
        await expect(label).toHaveText(kpi);
      });
    }
  });

  // Test complete crisis cycle
  test('Test full crisis cycle', async() => {
    // Guard: Skip if the initial check found Observer mode
    test.skip(isObserver, 'Skipping: Browser is in View-Only mode.');
    
    let current_state: number = h_n_4.at(-1) ?? 0; 
    let target_state: number = current_state;

    // Loop through full crisis cycle, starting from, and coming back to current state
    for (let i = 0; i <= 3; i++) {
      // Set target state
      target_state = target_state >= 3 ? 0 : target_state + 1;

      // Test step: get to target state by changing KPIs
      await test.step(`Test target state ${target_state}`, async () => {
        // Compute target KPIs
        const target_KPIs = await utils.computeTargetKPIs(target_state);
        
        // Apply KPIs, check state, loop thrice, if required, then fail
        const max_tries = 4;
        for (let j = 1; j <= max_tries; j++) {
          // Fail if attempts reached max tries 
          if (j === max_tries) {
            throw new Error(`Target state ${target_state} can not be achieved in ${max_tries - 1} attempts`);
          }
          
          // Apply target KPIs
          await utils.setTargetKPIs(sharedPage, target_KPIs);
          
          // Force refresh page
          await sharedPage.reload({ waitUntil: 'networkidle' });

          // Check updated state
          const hd = await utils.getHistoryData(sharedPage);  // Get history
          if (!hd) throw new Error('Could not find history data');  // Handle null
          if ('error' in hd) throw new Error(`Error: ${hd.error}`); // Handle error

          h_n_4 = hd.ntn;
          if (target_state === h_n_4.at(-1)) break; // Target state achieved, break
        }

        // Check status on mermaid block
        await utils.checkStatusOnMemraid(sharedPage, target_state);

      }); 
    }
  });

  // Reset to pre-test KPIs
  test('Reset to pre-test KPIs', async () => {
    // Guard: Skip if the initial check found Observer mode
    test.skip(isObserver, 'Skipping: Browser is in View-Only mode.');

    await test.step('Restore pre-test KPIs', async () => {
      const tk = ({...initialKpis} as unknown) as utils.TargetKPIs;
      await utils.setTargetKPIs(sharedPage, tk);    
    });
  });

  // Release lock & verify mode transition
  test('Release controller lock & verify observer mode', async () => {
    test.skip(isObserver, 'Skipping: No lock to release in Observer mode.');

    await test.step('Action: Release lock', async () => {
      const releaseBtn = sharedPage.getByRole('button', { name: 'Release Lock' });
      await expect(releaseBtn).toBeVisible();
      
      await releaseBtn.click();
    });

    await test.step('Verify: Observer mode active', async () => {
      // Check for the text change indicating the backend now sees us as Observer
      const modeLabel = sharedPage.locator('.mode');
      await expect(modeLabel).toContainText('Observer', { timeout: 2000 });
      
      // Verify the sliders are locked (disabled)
      const firstSlider = sharedPage.locator('input[type="range"]').first();
      await expect(firstSlider).toBeDisabled();
    });
  });
});
