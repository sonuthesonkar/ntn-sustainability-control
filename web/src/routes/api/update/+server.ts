/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

/**
 * @file +server.ts
 * @brief Streamlined KPI update gateway. 
 * Delegates validation and persistence to the gRPC service mesh.
 */

import { json } from '@sveltejs/kit';
import * as grpc from '$lib/grpc';

/**
 * @brief POST /api/update
 * Receive slider update -> Verify Lock -> Proxy to C++ Mesh.
 */
export async function POST({ request, cookies }) {
  const clientId = cookies.get('client_id');
  
  // Session check
  if (!clientId) {
    return json({ message: 'Session expired' }, { status: 401 });
  }

  // Lock check via gRPC
  const { owner_id } = await grpc.getLockStatus();
  if (owner_id && owner_id !== clientId) {
    return json({ message: 'Read-only: another controller active' }, { status: 403 });
  }

  // Payload extraction
  const { kpis, updated_kpi } = await request.json();

  // Convert all keys to lowercase to match .proto / C++ expectations
  const lc_kpis = Object.fromEntries(
      Object.entries(kpis).map(([k, v]) => [k.toLowerCase(), v])
  );

  /**
   * Mesh Handoff
   * Triggers DB_Svc -> Postgres -> Redis Pub: 'KPI_CHANGED'
   * Errors here bubble up to hooks.server.ts
   */
  const result = await grpc.updateKPI(lc_kpis, updated_kpi);

  if (!result.success) {
    throw new Error(`Mesh Update Failed: ${result.message}`);
  }

  return json({ status: 200 });
}
