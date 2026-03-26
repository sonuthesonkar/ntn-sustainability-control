/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

/**
 * @file grpc.ts
 * @brief Production-grade gRPC client for Svelte backend focusing on DBService.
 * Maps to the NTN Sustainability Architecture (MMD) for state management and KPI ingestion.
 */

import * as grpc from '@grpc/grpc-js';
import * as protoLoader from '@grpc/proto-loader';
import path from 'path';
import { logError } from './logger.server';

const PROTO_PATH = path.resolve('proto/db.proto');

// Type definitions

/**
 * @interface KPIValues
 * Matches the KPIValues message in db.proto for consistency across the distributed services mesh.
 */
export interface KPIValues {
  congestion: number;
  prb_util: number;
  traffic_load: number;
  ran_energy: number;
  carbon_intensity: number;
  isac_quality: number;
  mobility_rate: number;
}

/**
 * @interface DBStatus
 * Standard response for write/update operations in the C++ mesh.
 */
export interface DBStatus {
  success: boolean;
  message: string;
}

// Proto definitions & client initialization

const packageDefinition = protoLoader.loadSync(PROTO_PATH, {
  keepCase: true,
  longs: String,
  enums: String,
  defaults: true,
  oneofs: true,
  includeDirs: [path.resolve('.')]
});

const protoDescriptor: any = grpc.loadPackageDefinition(packageDefinition);
const dbProto = protoDescriptor.sustainability;

const client = new dbProto.DBService(
  process.env.DB_SERVICE_URL || 'db_service:50054',
  grpc.credentials.createInsecure()
);

/**
 * @brief Maps numeric gRPC codes to readable names for better debugging logs.
 */
const getGRPCErrorName = (code: number) => {
  return Object.keys(grpc.status).find(key => (grpc.status as any)[key] === code) || 'UNKNOWN';
};

/**
 * @brief Internal helper to wrap gRPC callbacks with enhanced error reporting and logging.
 */
const callRPC = <T>(method: string, request: any): Promise<T> => {
  return new Promise((resolve, reject) => {
    // Dynamic access to the client method based on the string name
    client[method](request, (err: grpc.ServiceError | null, response: T) => {
      if (err) {
        const errorName = getGRPCErrorName(err.code);
        
        // Construct a more descriptive error
        const enhancedError = new Error(
          `[gRPC ${errorName} (${err.code})]: DBService/${method} -> ${err.details || err.message}`
        );
        
        // Attach raw gRPC metadata to the Error object for downstream handling
        (enhancedError as any).grpcCode = err.code;
        (enhancedError as any).grpcDetails = err.details;
        (enhancedError as any).metadata = err.metadata;

        // Log the error to the server with a specific RPC context
        logError(enhancedError, `gRPC/${method}`);
        return reject(enhancedError);
      }
      resolve(response);  // Successfully received data from the C++ service
    });
  });
};

// Exported RPC methods

/**
 * @brief Attempts to acquire the controller lock for a specific client.
 * @param clientId Unique UUID for the browser session.
 */
export const getControllerLock = (clientId: string) => 
  callRPC<{ owner_id: string }>('GetControllerLock', { client_id: clientId });

/**
 * @brief Fetch the current lock owner without attempting to acquire it.
 */
export const getLockStatus = () => 
  callRPC<{ owner_id: string }>('GetLockStatus', {});

/**
 * @brief Fetch the 60-record sliding window history for GUI rendering.
 */
export const getPaddedHistory = (rcount: number = 60) => 
  callRPC<{ records: any[] }>('GetPaddedHistory', { rcount });

/**
 * @brief Pushes KPI updates from the UI to the DB Service.
 * This initiates the 'KPI_CHANGED' event bus trigger in the C++ mesh.
 * @param kpis The full object containing current slider values.
 * @param updatedField The specific ID of the KPI being adjusted.
 */
export const updateKPI = (kpis: KPIValues, updatedField: string): Promise<DBStatus> => 
  callRPC<DBStatus>('InsertKPI', { kpis, updated_field: updatedField });

/**
 * @brief Release the controller lock.
 * @param clientId Unique UUID for the user session.
 */
export const releaseLock = (clientId: string) => 
  callRPC<DBStatus>('ReleaseLock', { client_id: clientId });
