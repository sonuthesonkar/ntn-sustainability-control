/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/
import type { PageServerLoad } from './$types';
import pg from 'pg';
import { getPaddedHistory } from '$lib/db_queries';

/**
 * @brief Create a singleton pool
 */
const db = new pg.Pool({
  connectionString: process.env.DATABASE_URL
});

/**
 * @brief Server load function
 * @param param0 - cookies
 * @returns clientID, mode and history (from db, and padded)
 */
export const load: PageServerLoad = async ({ cookies }) => {
  try {
    let clientId = cookies.get('client_id');
    let mode = 'observer';
    let owner = '';

    if (!clientId) { // Set cookie
      clientId = crypto.randomUUID();
      cookies.set('client_id', clientId, {
        path: '/',
        httponly: true,
        sameSite: 'strict',
        maxAge: 60 * 60 * 1 // One hour
      });
    }

    const res = await db.query(`
      INSERT INTO controller (id, owner_id, acquired_at)
      VALUES (1, $1, now())
      ON CONFLICT (id) DO UPDATE 
      SET 
        owner_id = EXCLUDED.owner_id,
        acquired_at = EXCLUDED.acquired_at
      WHERE controller.owner_id IS NULL 
        OR controller.acquired_at < now() - INTERVAL '1 hour'
        OR controller.owner_id = EXCLUDED.owner_id
      RETURNING owner_id;
    `, [clientId]); // Upsert client id

    owner = res.rows[0]?.owner_id;
    mode = (owner === clientId) ? 'controller' : 'observer';
    const seq_len = 60;
    const history = await getPaddedHistory(seq_len);
    
    return {
      client_id: clientId,
      mode: mode,
      history: history
    };
  } catch (err: any) {
    throw err;
  }
};
