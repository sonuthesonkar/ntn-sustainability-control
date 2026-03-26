/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

/**
 * @file sustainer.cpp
 * @brief Orchestrator for the Sustainability Control Loop.
 * 
 * Listens to the KPI update trigger on redis stream, infer crisis score,
 * compute ntn state, and update db.
 */

#include <iostream>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <vector>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

#include <grpcpp/grpcpp.h>

#include "proto/sustainer.pb.h"
#include "proto/sustainer.grpc.pb.h"
#include "proto/db.grpc.pb.h"
#include "proto/crisis.grpc.pb.h"
#include "proto/ntn.grpc.pb.h"
#include "utils/redis_helper.hpp"
#include "utils/logger.hpp"

using namespace sustainability;

/**
 * @class SustainerCore
 * @brief The control engine responsible for monitoring the KPI changes, infering crisis, and rolling ntn state.
 */
class SustainerCore {
public:
    /**
     * @brief Initializes gRPC services with dedicated channels for the mesh.
     * @param db_url URL for the Database Gateway (db_service).
     * @param inf_url URL for the Inference Service (crisis_server).
     * @param ntn_url URL for the State Machine Service (ntn_service).
     */
    SustainerCore(const std::string& db_url, const std::string& inf_url, const std::string& ntn_url) {
        auto db_channel  = grpc::CreateChannel(db_url,  grpc::InsecureChannelCredentials());
        auto inf_channel = grpc::CreateChannel(inf_url, grpc::InsecureChannelCredentials());
        auto ntn_channel = grpc::CreateChannel(ntn_url, grpc::InsecureChannelCredentials());
        
        db_stub_  = DBService::NewStub(db_channel);
        inf_stub_ = CrisisService::NewStub(inf_channel);
        ntn_stub_ = NTNService::NewStub(ntn_channel);

        LOG_INFO("Service channels set | DB: {} | Inf: {} | NTN: {}", db_url, inf_url, ntn_url);
    }

    /**
     * @brief Synchronizes the local 59-record window with PostgreSQL/TimescaleDB.
     * @return true if hydration succeeded, false otherwise.
     */
    bool SyncWindow() {
        grpc::ClientContext context;
        HistoryRequest request;
        request.set_rcount(59); // Fetch latest 59 records from history. 60th record will be the current updates.
        HistoryResponse response;

        // SELECT from Postgres via DB_Svc
        grpc::Status status = db_stub_->GetPaddedHistory(&context, request, &response);
        
        if (status.ok()) {
            std::lock_guard<std::mutex> lock(window_mutex_);
            window_.clear();
            for (const auto& rec : response.records()) {
                window_.push_back(rec);
            }
            return true;
        } else {
            LOG_ERROR("Sync failed (DB Fetch) | {}", status.error_message());
            return false;
        }
    }

    /**
     * @brief Orchestrates the inference and state update loop.
     * Triggered by redis 'KPI_CHANGED' event.
     */
    void ProcessUpdateCycle(const nlohmann::json& snapshot) {
        // Force re-sync before processing
        if (!SyncWindow()) {
            LOG_ERROR("Sync failure | Aborting update cycle.");
            return;
        }

        std::lock_guard<std::mutex> lock(window_mutex_);
        if (window_.empty()) return;

        try {
            auto latest_kpis = snapshot["kpis"];    // Required for inferring crisis score
            auto state_data = snapshot["state_data"];   // Required for computing ntn state
            
            // Infer crisis score via crisis inference service
            CrisisRequest inf_req;
            inf_req.set_seq_len(60);
            inf_req.set_feature_dim(8);
            
            // Get 59 records from the sync window
            for (const auto& rec : window_) {
                const auto& k = rec.kpis();
                inf_req.add_kpi_sequence(k.congestion());
                inf_req.add_kpi_sequence(k.prb_util());
                inf_req.add_kpi_sequence(k.traffic_load());
                inf_req.add_kpi_sequence(k.ran_energy());
                inf_req.add_kpi_sequence(k.carbon_intensity());
                inf_req.add_kpi_sequence(k.isac_quality());
                inf_req.add_kpi_sequence(k.mobility_rate());
                inf_req.add_kpi_sequence(rec.crisis_score());
            }

            // Inject the 60th record from the redis snapshot
            inf_req.add_kpi_sequence(latest_kpis["congestion"].get<float>());
            inf_req.add_kpi_sequence(latest_kpis["prb_util"].get<float>());
            inf_req.add_kpi_sequence(latest_kpis["traffic_load"].get<float>());
            inf_req.add_kpi_sequence(latest_kpis["ran_energy"].get<float>());
            inf_req.add_kpi_sequence(latest_kpis["carbon_intensity"].get<float>());
            inf_req.add_kpi_sequence(latest_kpis["isac_quality"].get<float>());
            inf_req.add_kpi_sequence(latest_kpis["mobility_rate"].get<float>());
            inf_req.add_kpi_sequence(0.0f); // Placeholder for inference

            grpc::ClientContext inf_ctx;
            CrisisResponse inf_res;
            if (!inf_stub_->Evaluate(&inf_ctx, inf_req, &inf_res).ok()) {
                throw std::runtime_error("Crisis score inference request failed");
            }

            float latest_score = inf_res.crisis_scores(inf_res.crisis_scores_size() - 1);

            // Compute ntn state
            grpc::ClientContext ntn_ctx;
            NTNRequest ntn_req;
            ntn_req.set_score(latest_score);
            ntn_req.set_current_state(state_data["current_state"].get<int>());
            ntn_req.set_critical_count(state_data["critical_count"].get<int>());
            ntn_req.set_recovery_count(state_data["recovery_count"].get<int>());
            
            NTNResponse ntn_res;
            if (!ntn_stub_->ComputeState(&ntn_ctx, ntn_req, &ntn_res).ok()) {
                throw std::runtime_error("Compute ntn state request failed");
            }

            // Save updates (DB Service)
            grpc::ClientContext db_ctx;
            StateSaveRequest save_req;
            
            // Copy KPIs from the latest hydrated record
            auto* sr_kpis = save_req.mutable_kpis();
            sr_kpis->set_congestion(latest_kpis.value("congestion", 0.0f));
            sr_kpis->set_prb_util(latest_kpis.value("prb_util", 0.0f));
            sr_kpis->set_traffic_load(latest_kpis.value("traffic_load", 0.0f));
            sr_kpis->set_ran_energy(latest_kpis.value("ran_energy", 0.0f));
            sr_kpis->set_carbon_intensity(latest_kpis.value("carbon_intensity", 0.0f));
            sr_kpis->set_isac_quality(latest_kpis.value("isac_quality", 0.0f));
            sr_kpis->set_mobility_rate(latest_kpis.value("mobility_rate", 0.0f));
            
            // Copy latest crisis score and ntn state data
            save_req.set_score(latest_score);
            save_req.set_ntn_state(ntn_res.new_state());
            save_req.set_critical_count(ntn_res.new_critical_count());
            save_req.set_recovery_count(ntn_res.new_recovery_count());

            DBStatus db_stat;
            auto status = db_stub_->SaveProcessedState(&db_ctx, save_req, &db_stat);
            
            if (!status.ok() || !db_stat.success()) {
                std::string err_msg = db_stat.message().empty() ? status.error_message() : db_stat.message();
                throw std::runtime_error("State update request to DB failed | " + err_msg);
            }

        } catch (const std::exception& e) {
            LOG_CRITICAL("Update cycle failed | {}", e.what());
        }
    }

private:
    std::unique_ptr<DBService::Stub> db_stub_;
    std::unique_ptr<CrisisService::Stub> inf_stub_;
    std::unique_ptr<NTNService::Stub> ntn_stub_;
    std::deque<HistoryRecord> window_;
    std::mutex window_mutex_;
};

/**
 * @class SustainerServiceImpl
 * @brief Sustainability controller gRPC service, few manual ops.
 * 
 * Provides an interface for external services to force state updates 
 * or trigger manual batching logic.
 */
class SustainerServiceImpl final : public SustainerService::Service {
public:
    /**
     * @brief Constructs a new Sustainer Service Implementation.
     * @param s_core Shared pointer to the SustainerCore core logic.
     */
    explicit SustainerServiceImpl(std::shared_ptr<SustainerCore> s_core) : s_core_(s_core) {}

    /**
     * @brief RPC: Forces the sustainer to reload its state from the database.
     * Useful if Postgres data is manually modified and the sustainer's window is stale.
     */
    grpc::Status ResetHydration(grpc::ServerContext* context, 
                               const VoidMsg* request, 
                               SustainerStatus* response) override {
        LOG_INFO("Manual hydration reset requested.");
        bool success = s_core_->SyncWindow();
        
        response->set_success(success);
        response->set_message(success ? "State re-synchronized with DB" : "DB Sync failed");
        return grpc::Status::OK;
    }

private:
    std::shared_ptr<SustainerCore> s_core_;
};

/**
 * @brief Entry point: Configures mesh channels and starts the redis event subscriber.
 */
int main(int argc, char** argv) {
    logger::InitLogger("Sustainer"); // Initialize logger

    // Environment-based mesh configuration
    const char* env_db  = std::getenv("DB_SERVICE_URL");
    const char* env_inf = std::getenv("INFERENCE_URL");
    const char* env_ntn = std::getenv("NTN_SERVICE_URL");
    const char* env_port = std::getenv("PORT");
    
    std::string db_url  = (env_db)  ? env_db  : "db_service:50054";
    std::string inf_url = (env_inf) ? env_inf : "crisis_sevice:50051";
    std::string ntn_url = (env_ntn) ? env_ntn : "ntn_service:50053";
    std::string port = (env_port) ? env_port : "50051";

    std::string r_host = std::getenv("REDIS_HOST") ? std::getenv("REDIS_HOST") : "redis";
    int r_port = std::getenv("REDIS_PORT") ? std::stoi(std::getenv("REDIS_PORT")) : 6379;

    try {
        // Initialise sustainer core
        auto s_core = std::make_shared<SustainerCore>(db_url, inf_url, ntn_url);
        
        // Boot-time hydration
        if (!s_core->SyncWindow()) {
            LOG_ERROR("Initial hydration failed. Waiting for first redis trigger.");
        }

        // Subscribe to redis stream event trigger
        redis::RedisStreamListener listener;

        listener.listen(r_host, r_port, "KPI_CHANGED", [s_core](const std::vector<std::string>& batch) {
            if (batch.empty()) return;

            // There could be multiple KPI updates in the event 
            // Consolidate all KPI changes into one snapshot
            nlohmann::json consolidated_snapshot;
            bool initialized = false;

            for (const auto& raw_json : batch) {
                try {
                    auto current_event = nlohmann::json::parse(raw_json);
                    std::string updated_field = current_event.value("updated_kpi", "");

                    if (!initialized) {
                        // 1st in the batch: full initialization
                        consolidated_snapshot = current_event;
                        initialized = true;
                    } else {
                        // 2nd onwards: Apply the specific updates from this event
                        consolidated_snapshot["updated_kpi"] = updated_field;
                        consolidated_snapshot["state_data"] = current_event["state_data"];

                        // Update only the specific KPI that changed in this event
                        if (!updated_field.empty() && current_event["kpis"].contains(updated_field)) {
                            consolidated_snapshot["kpis"][updated_field] = current_event["kpis"][updated_field];
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Batch merge failed | {}", e.what());
                }
            }

            // Call inference ONCE with the fully updated state
            if (initialized) {
                s_core->ProcessUpdateCycle(consolidated_snapshot);
            }
        });

        // Start service
        std::string server_address("0.0.0.0:" + std::string(port));
        SustainerServiceImpl service(s_core);

        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        LOG_INFO("Service up | Waiting for redis event...");

        // Block the main thread until the process is terminated
        server->Wait();

    } catch (const std::exception& e) {
        LOG_CRITICAL("Service failed to start | {}", e.what());
        return 1;
    }

    return 0;
}
