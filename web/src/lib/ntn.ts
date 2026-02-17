/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

/**
 * @brief Update NTN state, critical and recovery counts.
 * @param ntn_state 
 * @param critical_count 
 * @param recovery_count 
 * @param score 
 * @returns updated ntn_state, critical_count, recovery_count
 */
export function computeNTN(ntn_state: number, critical_count: number, recovery_count: number, score: number) {
  try {
    let state = ntn_state, cc = critical_count, rc = recovery_count, s = score;
    let ntn_start = 0.6, ntn_cross = 0.8, critical_threshold = 0.9, critical_sustain_steps = 3;
    
    if (state < 3) {
      cc = s >= critical_threshold ? cc + 1 : 0;
      if (cc >= critical_sustain_steps) {
        state = 3; rc = 0;
      } else if (s >= ntn_cross) state = 2;
      else if (s >= ntn_start) state = 1;
      else state = 0;
    } else {
      rc = s < ntn_cross ? rc + 1 : 0;
      if (rc >= 2) {
        state = s >= ntn_start ? 1 : 0;
        cc = 0;
      }
    }
    return [state, cc, rc];
  } catch (err) {
    throw err;
  }
}
