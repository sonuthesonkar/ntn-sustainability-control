/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/
import { json } from '@sveltejs/kit';
import pg from 'pg';

/**
 * @brief Create singleton pool
 */
const db = new pg.Pool({
  connectionString: process.env.DATABASE_URL
});

/**
 * @brief Delete owner_id from db, effectively unlock db for new controller
 * @returns ok
 */
export async function POST() {
  try {
    await db.query(
      'UPDATE controller SET owner_id = null WHERE id = 1'
    );
    return json({ok: true});
  } catch (err: any) {
    throw err;
  }
}