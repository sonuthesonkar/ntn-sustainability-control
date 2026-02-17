/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/
import pg from 'pg';

const db = new pg.Pool({
  connectionString: process.env.DATABASE_URL
});
/**
 * @brief Fetch number of records from history table. Pad with zeros, if required.
 * @param rcount - number of records to be fetched
 * @returns rcount records
 */
export async function getPaddedHistory(rcount: number) {
    try {
        const { rows } = await db.query(
        'SELECT * FROM history ORDER BY ts DESC LIMIT 60'
        );

        let history = rows.reverse();

        // Pad if required.
        let seq_len = rcount;
        const pad_count =seq_len - history.length;
        if (pad_count > 0) {
        let pad_ts = new Date().toISOString(); // Required for graph.
        if (history.length > 0) {
            pad_ts = history[0].ts;
        }
        const padding = Array(pad_count).fill({
        ts: pad_ts, congestion: 0, prb_util: 0, traffic_load: 0, 
        ran_energy: 0, carbon_intensity: 0, isac_quality: 0, 
        mobility_rate: 0, crisis_score: 0, ntn_state: 0
        });
        history = [...padding, ...history];
        }
        return history;
    } catch (err) {
        throw err;
    }
}