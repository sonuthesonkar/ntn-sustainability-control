/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/
import { json } from '@sveltejs/kit';

/**
 * @brief Delete cookies, effectively unlock db for new controller
 * @param param0 - cookies
 * @returns ok
 */
export async function POST({cookies}) {
  try {
    cookies.delete('client_id', {path: '/'});
    return json({ok: true});
  } catch (err: any) {
    throw err;
  }
}