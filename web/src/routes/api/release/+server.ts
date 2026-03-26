/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

/**
 * @file +server.ts
 * @brief Endpoint to release the controller lock via the C++ Mesh.
 */

import { json, error } from '@sveltejs/kit';
import * as grpc from '$lib/grpc';

/**
 * @brief POST /api/release
 * @description 
 * 1. Identifies the current session via cookies.
 * 2. Commands DB_Svc to release the lock for this specific client.
 * 3. Errors bubble to hooks.server.ts for production logging.
 * 
 * @param event SvelteKit RequestEvent
 * @returns { ok: true } on successful release.
 */
export async function POST({ cookies }) {
  try {
    const clientId = cookies.get('client_id');
    
    if (!clientId) {
      throw error(401, { message: 'Session expired' });
    }

    /**
     * Call DB_Svc ReleaseLock gRPC
     * This handles the 'UPDATE controller SET owner_id = null' logic inside the Mesh.
     */
    const result = await grpc.releaseLock(clientId);

    if (!result.success) {
      // If the mesh rejects the release (e.g. client doesn't own the lock)
      throw error(400, { message: result.message || 'Mesh rejected lock release' });
    }

    return json({ ok: true });
    
  } catch (err: any) {
    // Re-throw to let hooks.server.ts handle the structured logging
    throw err;
  }
}
