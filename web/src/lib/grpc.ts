/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/
import grpc from '@grpc/grpc-js';
import loader from '@grpc/proto-loader';

/**
 * @brief Proto loader
 */
const def = loader.loadSync('proto/crisis.proto', {
  keepCase: true, // Critical, prevent protobuf snake_case to camelCase conversion
  longs: String,
  enums: String,
  defaults: true,
  oneofs: true
});

/**
 * @brief Load proto
 */
const pkg: any = grpc.loadPackageDefinition(def);

/**
 * @brief GRPC service instance
 */
const client = new pkg.CrisisService(
  process.env.GRPC_ADDR || 'grpc:50051',
  grpc.credentials.createInsecure()
);

/**
 * @brief Maps numeric gRPC codes to readable names for better debugging logs
 */
const getGRPCErrorName = (code: number) => {
  return Object.keys(grpc.status).find(key => (grpc.status as any)[key] === code) || 'UNKNOWN';
};

/**
 * @brief Infer the crisis score from the model.
 * @param sequence 
 * @returns crisis score
 */
export async function infer(sequence: number[][]) {
  
  const flattened = sequence.flat();
  
  return new Promise<number[]>((resolve, reject) => {
    client.Evaluate(
      {
        kpi_sequence: flattened,
        seq_len: 60,
        feature_dim: 8
      },
      (err: grpc.ServiceError | null, res: any) => {
        if (err) {
          // Construct a rich error message
          const errorName = getGRPCErrorName(err.code);
          const enhancedError = new Error(
            `[gRPC ${errorName} (${err.code})]: ${err.details || err.message}`
          );
          
          // Attach original metadata/details for advanced debugging
          (enhancedError as any).grpcCode = err.code;
          (enhancedError as any).grpcDetails = err.details;
          (enhancedError as any).metadata = err.metadata;

          return reject(enhancedError); // Return error
        }
        
        resolve(res.crisis_scores); // Success
      }
    );
  });
}
