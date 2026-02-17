/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/
import { json } from '@sveltejs/kit';
import { computeNTN } from '$lib/ntn';
import { infer } from '$lib/grpc';
import pg from 'pg';

/**
 * @brief Create singleton pool
 */
const db = new pg.Pool({
  connectionString: process.env.DATABASE_URL
});

/**
 * @brief Update db with kpi; infer crisis score, compute ntn state; update db
 * @param param0 - request, cookies 
 * @returns status 200, if all ok
 */
export async function POST({ request, cookies }) {
  try {
    let owner = '';
    const clientId = cookies.get('client_id');
    if (!clientId) {
      return json(
        { message: 'Session expired' }, 
        { status: 401 }
      );
    }
    
    const { rows: o_rows } = await db.query(
    'SELECT owner_id FROM controller WHERE id = 1'
    );

    if (!o_rows || o_rows.length === 0) {
      await db.query(
        'UPDATE controller SET owner_id = $1, acquired_at = now() WHERE id = 1',
        [clientId]
      );
      owner = clientId;
    } else {
      owner = o_rows[0]?.owner_id;
    }

    if (owner && owner !== clientId) {
      return json(
        { message: 'Read-only: another controller active' },
        { status: 403 }
      );
    }

    const { history, kpis, updated_kpi } = await request.json();
    const tbl = updated_kpi.toLowerCase();
    await db.query(
      `INSERT INTO ${tbl} (value) VALUES ($1)`, [kpis[updated_kpi]]
    );
    
    const seq_len = 60;
    let state_history = history.slice(1 - seq_len).map(r => [
      r.congestion, r.prb_util, r.traffic_load, r.ran_energy, r.carbon_intensity,
      r.isac_quality, r.mobility_rate, r.crisis_score
    ]);
    const current_state = [...Object.values(kpis), 0];
    const model_input = [...state_history, current_state];
    
    const output = await infer(model_input);
    const score = output.at(-1);
    
    const {rows} = await db.query(
      'SELECT * FROM ntn_state LIMIT 1'
    );

    let ntn_state = rows[0].ntn_state;
    let critical_count = rows[0].critical_count;
    let recovery_count = rows[0].recovery_count;
    [ntn_state, critical_count, recovery_count] = computeNTN(ntn_state, critical_count, recovery_count, score);
    
    await db.query(
        'UPDATE ntn_state SET ntn_state = $1, critical_count = $2, recovery_count = $3 WHERE id = 1',
        [ntn_state, critical_count, recovery_count]
      );

    await db.query(
      'INSERT INTO history VALUES (now(), $1, $2, $3, $4, $5, $6, $7, $8, $9)',
      [...Object.values(kpis), score, ntn_state]
    );

    return json({ status: 200 });
  } catch (err: any) {
    throw err;
  }
}
