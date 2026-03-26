/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

/**
 * @file redis_helper.hpp
 * @brief Thread-safe Redis Subscriber and Stream Listener using native hiredis.
 */
#pragma once
#include <hiredis/hiredis.h>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include "logger.hpp"

/**
 * @namespace redis
 * @brief High-performance messaging utilities for Redis-based distributed systems.
 */
namespace redis {

    /**
     * @class RedisProducer
     * @brief A lightweight producer for publishing messages to redis streams.
     */
    class RedisProducer {
    public:
        /**
         * @brief Constructs a new redis producer.
         */
        RedisProducer(const std::string& host, int port) : host_(host), port_(port) {}

        /**
         * @brief Appends a message to the redis stream 
         */
        void xadd(const std::string& stream, const std::string& payload, int max_len = 1000) {
            redisContext* c = redisConnect(host_.c_str(), port_);
            if (c && !c->err) {
                // Using MAXLEN ~ to keep the stream size under control automatically
                redisReply* reply = (redisReply*)redisCommand(c, 
                    "XADD %s MAXLEN ~ %d * payload %s", 
                    stream.c_str(), max_len, payload.c_str());
                
                if (reply) freeReplyObject(reply);
                redisFree(c);
            } else {
                LOG_ERROR("Redis connection failed for XADD");
                if (c) redisFree(c);
            }
        }
    private:
        std::string host_;  // Target redis hostname
        int port_;          // Target redis port
    };
    
    /**
     * @brief Subscriber for standard Redis Pub/Sub channels.
     */
    class RedisSubscriber {
    public:
        /**
         * @brief Function signature for processing incoming messages.
         */
        using Callback = std::function<void(const std::string& msg)>;

        /**
         * @brief Constructs the subscriber but does not connect immediately.
         */
        RedisSubscriber(const std::string& host, int port) 
            : host_(host), port_(port), stop_(false) {}

        /**
         * @brief Signals the worker thread to stop and joins it before destruction.
         */
        ~RedisSubscriber() {
            stop();
            if (worker_thread_.joinable()) worker_thread_.join();
        }

        /**
         * @brief Starts a background thread to subscribe to a Redis channel.
         * 
         * The worker thread will:
         * 1. Attempt to connect to Redis (with a 5s retry delay on failure).
         * 2. Issue the SUBSCRIBE command.
         * 3. Loop indefinitely, waiting for messages and executing the callback.
         * 4. Automatically reconnect if the connection is dropped.
         * 
         * @param channel The Redis Pub/Sub channel name to monitor.
         * @param cb The callback function to execute when a message is received.
         */
        void subscribe(const std::string& channel, Callback cb) {
            worker_thread_ = std::thread([this, channel, cb]() {
                while (!stop_) {
                    redisContext* c = redisConnect(host_.c_str(), port_);
                    if (c == nullptr || c->err) {
                        LOG_ERROR("Redis connection failed ({}). Retrying in 5s...", (c ? c->errstr : "alloc error"));
                        if (c) redisFree(c);
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        continue;
                    }

                    redisReply* reply = (redisReply*)redisCommand(c, "SUBSCRIBE %s", channel.c_str());
                    if (reply) freeReplyObject(reply);
                    LOG_INFO("Monitoring redis channel '{}'...", channel);

                    // Message processing loop
                    while (!stop_) {
                        void* _reply = nullptr;
                        // redisGetReply blocks until a message arrives or connection drops
                        if (redisGetReply(c, &_reply) == REDIS_OK && _reply != nullptr) {
                            redisReply* r = (redisReply*)_reply;
                            // Protocol check: Pub/Sub messages are arrays of 3 elements
                            if (r->type == REDIS_REPLY_ARRAY && r->elements == 3 && 
                                r->element[0]->str != nullptr && 
                                std::string(r->element[0]->str) == "message") {
                                
                                if (r->element[2]->str != nullptr) {
                                    // Extract the payload (index 2 in the Redis array)
                                    std::string payload(r->element[2]->str, r->element[2]->len);
                                    cb(payload);
                                }
                            }
                            freeReplyObject(r);
                        } else {
                            LOG_WARN("Redis connection lost. Reconnecting...");
                            break; // Exit inner loop to trigger reconnection
                        }
                    }
                    redisFree(c);
                }
            });
        }

        /**
         * @brief Sets the stop flag to true, causing the worker thread to exit.
         */
        void stop() { stop_ = true; }

    private:
        std::string host_;              // Redis server hostname
        int port_;                      // Redis server port
        std::thread worker_thread_;     // Background thread for the blocking subscribe loop
        std::atomic<bool> stop_;        // Thread-safe flag to signal shutdown
    };

    /**
     * @brief Listener for Redis Streams using blocking reads for real-time batching.
     */
    class RedisStreamListener {
    public:
        /**
         * @brief Callback type for processing a collection of messages.
         * @param payloads A vector of strings containing the "payload" field from each stream entry.
         */
        using BatchCallback = std::function<void(const std::vector<std::string>& payloads)>;

        /**
         * @brief Constructs the listener in a stopped state.
         */
        RedisStreamListener() : stop_(false) {}
        
        /**
         * @brief Triggers a stop and joins the worker thread to ensure a clean exit.
         */
        ~RedisStreamListener() { stop(); if (worker_.joinable()) worker_.join(); }

        /**
         * @brief Starts a background thread to consume messages from a Redis Stream.
         * 
         * Logic flow:
         * 1. Sets `last_id` to "$" to ignore historical data and only receive new messages.
         * 2. Connects to Redis with a 5s retry loop on failure.
         * 3. Uses `XREAD BLOCK 0` to wait indefinitely (push-like behavior) for data.
         * 4. Parses the complex Redis Stream nested array structure to extract "payload" fields.
         * 5. Updates `last_id` after every successful read to track progress.
         * 
         * @param host Redis server hostname.
         * @param port Redis server port.
         * @param stream The name of the stream to listen to.
         * @param cb Callback function to process batches of received payloads.
         */
        void listen(const std::string& host, int port, const std::string& stream, BatchCallback cb) {
            worker_ = std::thread([this, host, port, stream, cb]() {
                std::string last_id = "$"; // Only new messages from now on

                while (!stop_) {
                    redisContext* c = redisConnect(host.c_str(), port);
                    if (c == nullptr || c->err) {
                        LOG_ERROR("Redis connection failed ({}). Retrying in 5s...", (c ? c->errstr : "alloc error"));
                        if (c) redisFree(c);
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        continue;
                    }

                    LOG_INFO("Monitoring redis stream '{}' from ID '{}'...", stream.c_str(), last_id.c_str());

                    while (!stop_) {
                        // BLOCK 0 = push-like behavior (wait indefinitely for data)
                        redisReply* r = (redisReply*)redisCommand(c, "XREAD BLOCK 0 STREAMS %s %s", stream.c_str(), last_id.c_str());

                        if (r && r->type == REDIS_REPLY_ARRAY) {
                            std::vector<std::string> batch;
                            // Nesting: [ [stream_name, [ [id, [field, val, field, val] ] ] ] ]
                            redisReply* stream_block = r->element[0];
                            redisReply* messages = stream_block->element[1];

                            for (size_t i = 0; i < messages->elements; ++i) {
                                redisReply* entry = messages->element[i];
                                last_id = entry->element[0]->str; // Update to the current message ID

                                // Entry structure: [id, [field, value, field, value...]]
                                redisReply* kv_pairs = entry->element[1];
                                for (size_t j = 0; j < kv_pairs->elements; j += 2) {
                                    if (std::string(kv_pairs->element[j]->str) == "payload") {
                                        batch.push_back(kv_pairs->element[j+1]->str);
                                    }
                                }
                            }
                            if (!batch.empty()) cb(batch);
                        } else if (r == nullptr) {
                            LOG_WARN("Redis connection lost. Reconnecting...");
                            break;  // Exit to the reconnection loop
                        }
                        if (r) freeReplyObject(r);
                    }
                    redisFree(c);
                }
            });
        }

        /**
         * @brief Sets the stop flag. The worker will exit after the current blocking read completes.
         */
        void stop() { stop_ = true; }

    private:
        std::thread worker_;        // The background thread handling the blocking XREAD
        std::atomic<bool> stop_;    // Thread-safe flag to signal the loop to terminate 
    };
} // namespace redis
