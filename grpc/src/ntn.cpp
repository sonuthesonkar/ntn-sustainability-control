/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

/**
 * @file ntn.cpp
 * @brief gRPC service for NTN State Management.
 * 
 * Implements a 4-state transition model (0-3) with sustained-cycle 
 * requirements for entering and exiting the CRITICAL state.
 */

#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include <grpcpp/grpcpp.h>

#include "proto/ntn.grpc.pb.h"
#include "utils/logger.hpp"

using namespace sustainability;

/**
 * @namespace LogicConstants
 * @brief Encapsulates the specific thresholds and step requirements.
 */
namespace LogicConstants {
    constexpr float NTN_START          = 0.6f;  // Minimum crisis score to start NTN
    constexpr float NTN_CROSS          = 0.8f;  // Minimum crisis score to shift major traffic to NTN
    constexpr float CRITICAL_THRESHOLD = 0.9f;  // Crisis score (with sustained count) to switch to full NTN 

    constexpr int CRITICAL_SUSTAIN_STEPS = 3;   // Sustained count to switch to full NTN
    constexpr int RECOVERY_SUSTAIN_STEPS = 2;   // Sustained count to start recovery

    // NTN states
    constexpr int STATE_NORMAL = 0;
    constexpr int STATE_START = 1;
    constexpr int STATE_CROSS = 2;
    constexpr int STATE_CRITICAL = 3;   
}

/**
 * @class NTNServiceImpl
 * @brief Handles state computation requests for the sustainability mesh.
 * 
 * In future this same service can be used to actually switch the NTN 
 * nodes on/off, based on the crisis score.  
 * 
 */
class NTNServiceImpl final : public NTNService::Service {
public:
    /**
     * @brief Computes the next NTN state and counters.
     * @param context gRPC server context.
     * @param request Contains current ntn_state, critical_count, recovery_count, and score.
     * @param response Updated state and counters.
     * @return grpc::Status OK on success, INTERNAL on logical failure.
     */
    grpc::Status ComputeState(grpc::ServerContext* context, 
                             const NTNRequest* request, 
                             NTNResponse* response) override {
        try {
            // Load current context
            int state = request->current_state();
            int cc    = request->critical_count();
            int rc    = request->recovery_count();
            float s   = request->score();

            // Compute required new state
            if (state < LogicConstants::STATE_CRITICAL) {
                // Increment critical counter if above threshold, else reset
                cc = (s >= LogicConstants::CRITICAL_THRESHOLD) ? (cc + 1) : 0;

                if (cc >= LogicConstants::CRITICAL_SUSTAIN_STEPS) {
                    state = LogicConstants::STATE_CRITICAL;
                    rc = 0; // Reset recovery count upon entering critical
                } else if (s >= LogicConstants::NTN_CROSS) {
                    state = LogicConstants::STATE_CROSS;
                } else if (s >= LogicConstants::NTN_START) {
                    state = LogicConstants::STATE_START;
                } else {
                    state = LogicConstants::STATE_NORMAL;
                }
            } else {
                // Currently in STATE_CRITICAL (3). Check for recovery.
                rc = (s < LogicConstants::NTN_CROSS) ? (rc + 1) : 0;

                if (rc >= LogicConstants::RECOVERY_SUSTAIN_STEPS) {
                    // Transition back to lower states based on current score
                    state = (s >= LogicConstants::NTN_START) ? LogicConstants::STATE_START : LogicConstants::STATE_NORMAL;
                    cc = 0; // Reset critical counter
                }
            }

            // Populate response
            response->set_new_state(state);
            response->set_new_critical_count(cc);
            response->set_new_recovery_count(rc);

            return grpc::Status::OK;

        } catch (const std::exception& e) {
            LOG_ERROR("Compute state failed | {}", e.what());
            return grpc::Status(grpc::StatusCode::INTERNAL, "Logical computation error");
        }
    }
};

/**
 * @brief Entry point for the NTN state machine service.
 */
int main(int argc, char** argv) {
    logger::InitLogger("NTN Service"); // Initialize logger

    // Get environment variables
    const char* env_port = std::getenv("PORT");
    std::string port = (env_port) ? env_port : "50051";
    std::string server_address("0.0.0.0:" + port);

    // Start service
    try {
        NTNServiceImpl service;
        grpc::ServerBuilder builder;

        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);

        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        if (!server) {
            std::cerr << "FATAL: NTN Service failed to bind to " << server_address << std::endl;
            return 1;
        }

        LOG_INFO("Service up and listening on {}", server_address);
        server->Wait();

    } catch (const std::exception& e) {
        LOG_CRITICAL("Service failed to start | {}", e.what());
        return 1;
    }

    return 0;
}
