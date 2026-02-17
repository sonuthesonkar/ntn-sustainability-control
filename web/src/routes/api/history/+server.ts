/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/
import { json } from '@sveltejs/kit';
import pg from 'pg';
import { getPaddedHistory } from '$lib/db_queries';

/**
 * @brief Create singleton pool
 */
const db = new pg.Pool({
  connectionString: process.env.DATABASE_URL
});

/**
 * @brief Fetch history records from db, pad if needed
 * @returns padded history
 */
export async function GET() {
  try {
    
    const seq_len = 60;
    const history = await getPaddedHistory(seq_len); 
      
    return json(history, {
      headers: {
        'cache-control': 'no-store'
      }
    });
  } catch (err: any) {
    throw err;
  }
}
