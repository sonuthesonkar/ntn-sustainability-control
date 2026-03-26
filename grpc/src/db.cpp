/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

/**
 * @file db.cpp
 * @brief gRPC service bridging TimescaleDB and Redis Pub/Sub/Stream.
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

#include <pqxx/pqxx>
#include <hiredis/hiredis.h>
#include <grpcpp/grpcpp.h>

#include "proto/db.grpc.pb.h"
#include "utils/logger.hpp"
#include "utils/redis_helper.hpp"

using namespace sustainability;

/**
 * @brief DB Schema
 */
namespace Schema {
    constexpr auto TBL_HISTORY    = "history";
    constexpr auto TBL_NTN_STATE  = "ntn_state";
    constexpr auto TBL_CONTROLLER = "controller";

    constexpr auto COL_ID          = "id";
    constexpr auto COL_TS          = "ts";
    constexpr auto COL_CONGESTION  = "congestion";
    constexpr auto COL_PRB         = "prb_util";
    constexpr auto COL_TRAFFIC     = "traffic_load";
    constexpr auto COL_ENERGY      = "ran_energy";
    constexpr auto COL_CARBON      = "carbon_intensity";
    constexpr auto COL_ISAC        = "isac_quality";
    constexpr auto COL_MOBILITY    = "mobility_rate";
    constexpr auto COL_SCORE       = "crisis_score";
    constexpr auto COL_STATE       = "ntn_state";

    constexpr auto COL_CRITICAL_CNT = "critical_count";
    constexpr auto COL_RECOVERY_CNT = "recovery_count";

    constexpr auto COL_OWNER       = "owner_id";
    constexpr auto COL_ACQUIRED    = "acquired_at";
}

/**
 * @brief Redis channels
 */
namespace Events {
    constexpr auto CH_KPI_CHANGED   = "KPI_CHANGED";
    constexpr auto CH_UPDATE_GUI  = "UPDATE_GUI";
}

/**
 * @class DBServiceImpl
 * @brief Implements DBService gRPC interface with integrated redis event publishing.
 */
class DBServiceImpl final : public DBService::Service {
public:
    DBServiceImpl(std::string pg_url, std::string redis_host, int redis_port) 
        : pg_conn_str_(std::move(pg_url)), r_host_(std::move(redis_host)), r_port_(redis_port) {
        validate_connections();
    }

    ~DBServiceImpl() {
        if (redis_ctx_) redisFree(redis_ctx_);
    }

    /**
     * @brief Atomic DB lock acquisition/renewal. Required in multiple web GUI scenarios.
     */
    grpc::Status GetControllerLock(grpc::ServerContext* context, const LockRequest* request, LockResponse* response) override {
        try {
            // Get DB connection
            pqxx::connection conn(pg_conn_str_);
            pqxx::work txn(conn);

            // Attempt to acquire global contoller lock in the DB, through coditional upsert
            std::stringstream sql;
            sql << "INSERT INTO " << Schema::TBL_CONTROLLER << " (id, " << Schema::COL_OWNER << ", " << Schema::COL_ACQUIRED << ") "
                << "VALUES (1, $1, now()) "
                << "ON CONFLICT (id) DO UPDATE SET "
                << Schema::COL_OWNER << " = EXCLUDED." << Schema::COL_OWNER << ", "
                << Schema::COL_ACQUIRED << " = EXCLUDED." << Schema::COL_ACQUIRED << " "
                << "WHERE " << Schema::TBL_CONTROLLER << "." << Schema::COL_OWNER << " IS NULL "
                << "OR " << Schema::TBL_CONTROLLER << "." << Schema::COL_ACQUIRED << " < now() - INTERVAL '1 hour' "
                << "OR " << Schema::TBL_CONTROLLER << "." << Schema::COL_OWNER << " = EXCLUDED." << Schema::COL_OWNER
                << " RETURNING " << Schema::COL_OWNER;

            auto res = txn.exec_params(sql.str(), request->client_id());
            txn.commit();

            if (!res.empty()) {
                // Lock successfully acquired or renewed
                response->set_owner_id(res[0][Schema::COL_OWNER].as<std::string>());
            } else {
                // WHERE clause failed: Lock is held by someone else and still valid.
                // Fetch the current owner to inform the client.
                pqxx::read_transaction read_txn(conn);
                auto current = read_txn.exec(std::string("SELECT ") +
                                            Schema::COL_OWNER + " FROM " + Schema::TBL_CONTROLLER + " WHERE id = 1");
                
                if (!current.empty()) {
                    response->set_owner_id(current[0][0].as<std::string>());
                }
            }
            return grpc::Status::OK;
        } catch (const std::exception& e) {
            LOG_ERROR("Get lock failed | {}", e.what());
            return grpc::Status(grpc::StatusCode::INTERNAL, "Get lock failed");
        }
    }

    /**
     * @brief Ingests KPI and publishes 'KPI_CHANGED' trigger for sustainer.
     */
    grpc::Status InsertKPI(grpc::ServerContext* context, const KPIUpdateRequest* request, DBStatus* response) override {
        try {
            // Get DB connection
            pqxx::connection conn(pg_conn_str_);
            pqxx::work txn(conn);

            // Update relevant KPI table
            std::string tbl = request->updated_field();
            std::transform(tbl.begin(), tbl.end(), tbl.begin(), ::tolower);

            float val = get_kpi_value(request->kpis(), tbl);
            
            std::stringstream sql;
            sql << "INSERT INTO " << tbl << " (value) VALUES ($1)";
            txn.exec_params(sql.str(), val);
            
            // Build the Snapshot JSON (The "Event-Driven State")
            // We use the values already present in the gRPC request 
            // because they represent the "New State" the UI just sent.
            const auto& k = request->kpis();
            nlohmann::json snapshot;
            snapshot["updated_kpi"] = request->updated_field();
            snapshot["kpis"] = {
                {"congestion", k.congestion()},
                {"prb_util", k.prb_util()},
                {"traffic_load", k.traffic_load()},
                {"ran_energy", k.ran_energy()},
                {"carbon_intensity", k.carbon_intensity()},
                {"isac_quality", k.isac_quality()},
                {"mobility_rate", k.mobility_rate()}
            };

            // Fetch the latest NTN State data
            pqxx::result r = txn.exec(std::string("SELECT * FROM ") + Schema::TBL_NTN_STATE + " WHERE " + Schema::COL_ID + " = 1");

            // Add the state data
            if (!r.empty()) {
                snapshot["state_data"] = {
                    {"current_state", r[0][Schema::COL_STATE].as<int>()},
                    {"critical_count", r[0][Schema::COL_CRITICAL_CNT].as<int>()},
                    {"recovery_count", r[0][Schema::COL_RECOVERY_CNT].as<int>()}
                };
            }

            txn.commit();   // All db work done

            // Publish call to an XADD (Stream Append)
            redis::RedisProducer producer(r_host_, r_port_);
            producer.xadd(Events::CH_KPI_CHANGED, snapshot.dump());

            response->set_success(true);
            return grpc::Status::OK;
        } catch (const std::exception& e) {
            LOG_ERROR("InsertKPI failed | {}", e.what());
            response->set_success(false);
            response->set_message(e.what());
            return grpc::Status::OK;
        }
    }

    /**
     * @brief History retrieval, with zero-padding if records are less than the count in request.
     */
    grpc::Status GetPaddedHistory(grpc::ServerContext* context, const HistoryRequest* request, HistoryResponse* response) override {
        try {
            // Get DB connection
            pqxx::connection conn(pg_conn_str_);
            pqxx::read_transaction txn(conn);

            // Get latest rcount records from the history, in time descending order
            std::stringstream sql;
            sql << "SELECT * FROM " << Schema::TBL_HISTORY << " ORDER BY " << Schema::COL_TS << " DESC LIMIT " << request->rcount();
            auto res = txn.exec(sql.str());

            std::vector<pqxx::row> rows(res.begin(), res.end());
            
            // Reverse the order, latest in the end
            std::reverse(rows.begin(), rows.end());

            // Calculate required padding 
            int pad_count = request->rcount() - (int)rows.size();
            std::string pad_ts = rows.empty() ? get_iso_now() : rows[0][Schema::COL_TS].as<std::string>();

            // Add padding
            for (int i = 0; i < pad_count; ++i) serialize_row(response->add_records(), pad_ts, nullptr);
            
            // Add records
            for (const auto& row : rows) serialize_row(response->add_records(), row[Schema::COL_TS].as<std::string>(), &row);

            return grpc::Status::OK;
        } catch (const std::exception& e) {
            LOG_ERROR("Get history failed | {}", e.what());
            return grpc::Status(grpc::StatusCode::INTERNAL, "Get history failed");
        }
    }

    /**
     * @brief Persists sustainer results and publishes 'UPDATE_GUI' trigger.
     */
    grpc::Status SaveProcessedState(grpc::ServerContext* context, const StateSaveRequest* request, DBStatus* response) override {
        try {
            // GEt DB connection
            pqxx::connection conn(pg_conn_str_);
            pqxx::work txn(conn);

            // Update history in the DB
            std::stringstream h_sql;
            h_sql << "INSERT INTO " << Schema::TBL_HISTORY << " (" << Schema::COL_TS << "," << Schema::COL_CONGESTION << ","
                  << Schema::COL_PRB << "," << Schema::COL_TRAFFIC << "," << Schema::COL_ENERGY << "," << Schema::COL_CARBON << ","
                  << Schema::COL_ISAC << "," << Schema::COL_MOBILITY << "," << Schema::COL_SCORE << "," << Schema::COL_STATE
                  << ") VALUES (now(),$1,$2,$3,$4,$5,$6,$7,$8,$9)";
            
            txn.exec_params(h_sql.str(), request->kpis().congestion(), request->kpis().prb_util(), request->kpis().traffic_load(),
                           request->kpis().ran_energy(), request->kpis().carbon_intensity(), request->kpis().isac_quality(),
                           request->kpis().mobility_rate(), request->score(), request->ntn_state());

            // Update state in the DB
            std::stringstream s_sql;
            s_sql << "UPDATE " << Schema::TBL_NTN_STATE << " SET " << Schema::COL_STATE << "=$1," << Schema::COL_CRITICAL_CNT << "=$2,"
                  << Schema::COL_RECOVERY_CNT << "=$3 WHERE id=1";
            txn.exec_params(s_sql.str(), request->ntn_state(), request->critical_count(), request->recovery_count());

            txn.commit();

            // Publish 'UPDATE_GUI' trigger
            publish_event(Events::CH_UPDATE_GUI, "state_changed");

            response->set_success(true);
            return grpc::Status::OK;
        } catch (const std::exception& e) {
            LOG_ERROR("SaveState failed | {}", e.what());
            response->set_success(false);
            response->set_message(e.what());
            return grpc::Status::OK;
        }
    }

    /**
     * @brief Gets current state from the DB.
     */
    grpc::Status GetNTNState(grpc::ServerContext* context, const GetNTNStateRequest* request, NTNResponse* response) override {
        try {
            // Get DB connection
            pqxx::connection conn(pg_conn_str_);
            pqxx::read_transaction txn(conn);
            
            // Get current state 
            auto res = txn.exec("SELECT * FROM ntn_state LIMIT 1");
            if (!res.empty()) {
                response->set_new_state(res[0]["ntn_state"].as<int>());
                response->set_new_critical_count(res[0]["critical_count"].as<int>());
                response->set_new_recovery_count(res[0]["recovery_count"].as<int>());
            }
            return grpc::Status::OK;
        } catch (...) { return grpc::Status(grpc::StatusCode::INTERNAL, "DB Error"); }
    }

    /**
     * @brief Gets owner holding the control lock.
     */
    grpc::Status GetLockStatus(grpc::ServerContext* context, const GetLockStatusRequest* request, GetLockStatusResponse* response) override {
        try {
            // Get DB connection
            pqxx::connection conn(pg_conn_str_);
            pqxx::read_transaction txn(conn);
            
            // Get current lock owner
            auto res = txn.exec("SELECT owner_id FROM controller WHERE id=1");
            if (!res.empty() && !res[0][0].is_null()) response->set_owner_id(res[0][0].as<std::string>());
            return grpc::Status::OK;
        } catch (...) { return grpc::Status(grpc::StatusCode::INTERNAL, "DB Error"); }
    }

    /**
     * @brief Releases control lock.
     */
    grpc::Status ReleaseLock(grpc::ServerContext* context, const LockRequest* request, DBStatus* response) override {
        try {
            // Get DB connection
            pqxx::connection conn(pg_conn_str_);
            pqxx::work txn(conn);

            // Release controller lock
            txn.exec("UPDATE controller SET owner_id=NULL WHERE id=1");
            txn.commit();
            response->set_success(true);
            return grpc::Status::OK;
        } catch (...) { return grpc::Status(grpc::StatusCode::INTERNAL, "DB Error"); }
    }

private:
    std::string pg_conn_str_, r_host_;
    int r_port_;
    redisContext* redis_ctx_ = nullptr;

    /**
     * @brief Checks DB and redis connections.
     */
    void validate_connections() {
        // Check DB connection
        pqxx::connection c(pg_conn_str_);
        if (!c.is_open()) {
            throw std::runtime_error("DB connection failed");
        }

        // Check redis connection
        redis_ctx_ = redisConnect(r_host_.c_str(), r_port_);
        if (!redis_ctx_ || redis_ctx_->err) {
            std::string err_msg = "Redis connection failed | ";
            err_msg += (redis_ctx_ ? redis_ctx_->errstr : "Memory allocation error");
            
            // Clean up if redis_ctx was allocated but failed
            if (redis_ctx_) redisFree(redis_ctx_); 
            
            throw std::runtime_error(err_msg);
        }
    }

    /**
     * @brief Publishes redis event.
     */
    void publish_event(const std::string& channel, const std::string& msg) {
        if (!redis_ctx_) return;
        redisReply* reply = (redisReply*)redisCommand(redis_ctx_, "PUBLISH %s %s", channel.c_str(), msg.c_str());
        if (reply) freeReplyObject(reply);
    }

    /**
     * @brief Gets KPI value based on the field.
     */
    float get_kpi_value(const KPIValues& k, const std::string& field) {
        if (field == "congestion") return k.congestion();
        if (field == "prb_util") return k.prb_util();
        if (field == "traffic_load") return k.traffic_load();
        if (field == "ran_energy") return k.ran_energy();
        if (field == "carbon_intensity") return k.carbon_intensity();
        if (field == "isac_quality") return k.isac_quality();
        if (field == "mobility_rate") return k.mobility_rate();
        return 0.0f;
    }

    /**
     * @brief Gets current time in ISO 8601 format (e.g., 2023-10-27T14:30:05Z).
     */
    std::string get_iso_now() {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&t), "%FT%TZ");
        return ss.str();
    }

    /**
     * @brief Translates db row into the protobuf HistoryRecord.
     */
    void serialize_row(HistoryRecord* rec, const std::string& ts, const pqxx::row* row) {
        rec->set_ts(ts);
        auto* k = rec->mutable_kpis();
        if (!row) {
            rec->set_crisis_score(0.0f); rec->set_ntn_state(0);
            k->set_congestion(0.0f); k->set_prb_util(0.0f); k->set_traffic_load(0.0f);
            k->set_ran_energy(0.0f); k->set_carbon_intensity(0.0f); k->set_isac_quality(0.0f); k->set_mobility_rate(0.0f);
        } else {
            rec->set_crisis_score((*row)[Schema::COL_SCORE].as<float>());
            rec->set_ntn_state((*row)[Schema::COL_STATE].as<int>());
            k->set_congestion((*row)[Schema::COL_CONGESTION].as<float>());
            k->set_prb_util((*row)[Schema::COL_PRB].as<float>());
            k->set_traffic_load((*row)[Schema::COL_TRAFFIC].as<float>());
            k->set_ran_energy((*row)[Schema::COL_ENERGY].as<float>());
            k->set_carbon_intensity((*row)[Schema::COL_CARBON].as<float>());
            k->set_isac_quality((*row)[Schema::COL_ISAC].as<float>());
            k->set_mobility_rate((*row)[Schema::COL_MOBILITY].as<float>());
        }
    }
};

/**
 * @brief Entry point for the DB Service.
 */
int main() {
    logger::InitLogger("DB Service"); // Initialize logger

    // Get environment variables
    const char* pg_url = std::getenv("DB_URL");
    const char* r_host = std::getenv("REDIS_HOST") ? std::getenv("REDIS_HOST") : "redis";
    int r_port = std::getenv("REDIS_PORT") ? std::stoi(std::getenv("REDIS_PORT")) : 6379;
    const char* port = std::getenv("PORT") ? std::getenv("PORT") : "50051";

    if (!pg_url) return 1;

    // Start service
    try {
        DBServiceImpl service(pg_url, r_host, r_port);
        grpc::ServerBuilder builder;
        builder.AddListeningPort("0.0.0.0:" + std::string(port), grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        LOG_INFO("Service up with redis triggers enabled. Listening on: 0.0.0.0:" + std::string(port) + "");
        builder.BuildAndStart()->Wait();
    } catch (const std::exception& e) {
        return 1;
    }
    return 0;
}
