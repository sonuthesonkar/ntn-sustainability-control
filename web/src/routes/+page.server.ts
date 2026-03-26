/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

/**
 * @file +page.server.ts
 * @brief Server load function for the web dashboard.
 * 
 * Implements session management, controller locking, 
 * and initial history hydration using the C++ Mesh gRPC Gateway.
 */

import type { PageServerLoad } from './$types';
import * as grpc from '$lib/grpc';

/**
 * @brief SvelteKit Server Load Function.
 * @param event - Contains cookies for session tracking and client identification.
 * @returns { clientId, mode, history } 
 * @note Errors are handled globally via hooks.server.ts.
 */
export const load: PageServerLoad = async ({ cookies }) => {
  try {
    let clientId = cookies.get('client_id');
    let mode: 'controller' | 'observer' = 'observer';
    let owner: string = '';

    // Session & identity management
    if (!clientId) {
      clientId = crypto.randomUUID();
      cookies.set('client_id', clientId, {
        path: '/',
        httpOnly: true,
        sameSite: 'strict',
        maxAge: 60 * 60 * 1, // 1-hour session expiry
        secure: false        // Set to true in production with TLS/SSL
      });

      // Attempt to acquire the controller lock.
      const lockRes = await grpc.getControllerLock(clientId);
      owner = lockRes.owner_id;
    } else {
      // (Alternative): Fetch current owner without acquisition attempt.
      const statusRes = await grpc.getLockStatus();
      owner = statusRes.owner_id;
    }

    // Control mode determination
    mode = (owner === clientId) ? 'controller' : 'observer';

    // Hydrate initial 60-record history for GUI rendering
    const SEQ_LEN = 60;
    const historyRes = await grpc.getPaddedHistory(SEQ_LEN);
    
    return {
      client_id: clientId,
      mode: mode,
      history: historyRes.records
    };

  } catch (err: any) {
    throw err;  // hooks.server to handle this
  }
};
