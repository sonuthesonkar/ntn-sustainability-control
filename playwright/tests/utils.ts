/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/
import { Page, expect } from '@playwright/test';

/**
 * Utility functions and custom types for the test suite.
 * 
 * Provides shared logic for data extraction, and complex test flows. 
 */


/** Custom type for history data */
export type HistoryData = {
  ntn: number[];
} | {
  error: string;
} | null;

/** Function to return the history data */
export async function getHistoryData(page: Page): Promise<HistoryData> {
  return await page.evaluate(() => {
    // Find all script tags
    const scripts = Array.from(document.querySelectorAll('script'));
      
    // Find the one containing the serialized SvelteKit data
     const dataScript = scripts.find(s => s.textContent?.includes('history:'));
     
    if (!dataScript || !dataScript.textContent) return null;

    try {
      // SvelteKit often uses a specific format like: 
      // { type: "data", data: { ... } }
      // We use a regex or eval-like approach to grab the object inside.
      // WARNING: Direct 'eval' is risky; this specific regex approach is safer for extraction.
      const rawText = dataScript.textContent;
        
      // Look for the part starting with {type:"data"...
      const match = rawText.match(/\{type:"data",data:\{[\s\S]*?\}\}/);
      if (!match) return null;

      // Clean the string into a valid JSON format if possible, 
      // or use a Function constructor for a controlled evaluation
      const extracted = new Function(`
        const a = { ntn_state: 0 };
        const b = new Date();
        return ${match[0]};
      `)();
      const history = extracted.data.history;
      return {
        ntn: history.slice(-4).map((h: any) => h.ntn_state)
      };
    } catch (e: any) {
      return { error: e.message };
    }
  });
}

/** Interface for tyarget KPIs */
export interface TargetKPIs {
  Congestion: number; 
  PRB_Util: number; 
  Traffic_Load: number;
  RAN_Energy: number; 
  Carbon_Intensity: number;
  ISAC_Quality: number;
  Mobility_Rate: number
}

/** Compute KPIs to acheive target ntn state */
export async function computeTargetKPIs(target_state: number): Promise<TargetKPIs> {
  const ntn_start_limit = 0.6, ntn_cross_limit = 0.8, ntn_full_limit = 0.9; // Start thresholds for ntn switch
  const thresholds = [0, ntn_start_limit, ntn_cross_limit, ntn_full_limit, 1];

  /**
   * target_state:
   * 0 - No ntn - crisis score 0 - 0.6
   * 1 - ntn start - crisis score 0.6 - 0.8
   * 2 - ntn cross - crisis score 0.8 - 0.9
   * 3 - ntn full - crisis score 0.9 - 1
   */

  // Compute target crisis score, try and avoid near boundaries
  const mid_score = (thresholds[target_state + 1] + thresholds[target_state]) / 2;
  const target_score = mid_score; // Target mid score in the range
  
  // Compute target KPIs
  const congestion = Math.min(target_score, 1); // Direct proportional to crisis
  const prb_util = Math.min(0.4 + (0.6 * target_score), 1); // 40% base usage
  const traffic = Math.min(0.3 + (0.7 * target_score), 1); // 30% base usage
  const ran_energy = Math.min(0.2 + (0.8 * target_score), 1); // 20% base usage
  const carbon = Math.min((0.5 * target_score), 1); // Proportional to crisis, but not 1:1
  const isac_quality = Math.min(1 - target_score, 1); // Inverse to crisis
  const mobility = Math.min(1 - (0.8 * target_score), 1); // Inverse to crisis but slower than isac

  return {
    Congestion: Number(congestion.toFixed(2)), 
    PRB_Util: Number(prb_util.toFixed(2)), 
    Traffic_Load: Number(traffic.toFixed(2)),
    RAN_Energy: Number(ran_energy.toFixed(2)), 
    Carbon_Intensity: Number(carbon.toFixed(2)),
    ISAC_Quality: Number(isac_quality.toFixed(2)),
    Mobility_Rate: Number(mobility.toFixed(2))
  };
}

/** Set target KPIs */
export async function setTargetKPIs(page: Page, tk: TargetKPIs): Promise<void> {
  // Loop through the KPIs
  for (const [k, v] of Object.entries(tk)) {
    const container = page.locator('.slider-container').filter({ hasText: k });
    const slider = container.locator('input[type="range"]');
    const display = container.locator('.caption').last();
    
    // Use fill (auto handles input and change event dispatch)
    await slider.fill(v.toString());
    
    // Wait for the background fetches triggered by the fill to complete
    await page.waitForResponse(resp => 
      resp.url().includes('/api/update') && resp.status() === 200
    );
    
    // Verify Svelte-to-gRPC sync
    await expect(display).toHaveText(v.toFixed(2)); // Compare string, as seen to user
  }
}

/** Check status on mermaid block diagram */
export async function checkStatusOnMemraid(page: Page, state: number): Promise<void> {
  const ntn_states = [" ", "! NTNStart !", "! NTNCross !", "! NTNFull !"];
  
  // Locate the content area containing the CSS variable
  const contentArea = page.locator('.right-col .content-area');

  // Get the value of the custom CSS property '--ntn-text'
  const statusText = await contentArea.evaluate((el) => {
    // getComputedStyle picks up the variable even if it's set via style attribute
    return window.getComputedStyle(el).getPropertyValue('--ntn-text').trim();
  });

  // Assert (Note: getPropertyValue usually includes the quotes if they were in the CSS)
  // If variable is "'Active'", the string will be "'Active'"
  expect(statusText).toContain(ntn_states[state]);
}