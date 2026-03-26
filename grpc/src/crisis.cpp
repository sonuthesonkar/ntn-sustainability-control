/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

#include <grpcpp/grpcpp.h>
#include "crisis.grpc.pb.h"
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <iostream>
#include <string>
#include "utils/logger.hpp"

using namespace sustainability;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

/**
 * @brief Implementation of the gRPC CrisisService.
 * 
 * This service handles real-time inference of the crisis_score based 
 * on the input sustainability KPIs metrics.
 * 
 * @note This class is marked 'final' to prevent further inheritance.
 */
class CrisisServiceImpl final : public CrisisService::Service {
public:
  
  /**
   * @brief Constructs the Crisis Service and loads the ML model.
   * 
   * Initialises the gRPC service implementation by loading the required 
   * inference model from the specified filesystem path.
   * 
   * @param model_path The absolute or relative path to the .onnx file.
   */
  CrisisServiceImpl(const std::string& model_path)
    : env(ORT_LOGGING_LEVEL_WARNING, "ort_log"),
      session(env, model_path.c_str(), CreateOptimizedOptions()) 
  {
    // FIX: Store the strings in std::string to own the memory
    Ort::AllocatorWithDefaultOptions allocator;
    auto in_ptr = session.GetInputNameAllocated(0, allocator);
    auto out_ptr = session.GetOutputNameAllocated(0, allocator);
    
    input_name_str = in_ptr.get();
    output_name_str = out_ptr.get();

    LOG_INFO("Inference engine ready | Model: {} | Input: {} | Output: {}",
                  model_path, input_name_str, output_name_str);
  }

  /**
   * @brief Evaluates current sustainability metrics to determine crisis levels.
   * 
   * This method implements the core logic of the CrisisService. It processes 
   * incoming KPI data (congestion, energy, etc.), performs inference using 
   * the loaded model, and populates the response with a crisis score.
   * 
   * @param context gRPC metadata and control object (currently reserved for future use).
   * @param req     Pointer to the CrisisRequest containing the latest KPIs metrics.
   * @param res     Pointer to the CrisisResponse to be populated with evaluation results.
   * 
   * @return 'grpc::Status::OK' on success, or grpc status code on error. 
   */
  Status Evaluate(ServerContext* context,
                  const CrisisRequest* req,
                  CrisisResponse* res) override
  {
    try {
        const int64_t T = req->seq_len();
        const int64_t F = req->feature_dim();
        const int64_t expected_size = T * F;

        // Validation + Logging
        if (req->kpi_sequence_size() != expected_size) {
          std::string errmsg = fmt::format("Invalid kpi_sequence size | Got: {} | Expected: {}",
                                            req->kpi_sequence_size(), expected_size);
          LOG_ERROR(errmsg);  
            return Status(grpc::StatusCode::INVALID_ARGUMENT, errmsg);
        }

        std::array<int64_t, 3> shape{1, T, F};

        // Wrap Tensor Creation
        Ort::Value input = Ort::Value::CreateTensor<float>(
          mem,
          const_cast<float*>(req->kpi_sequence().data()),
          req->kpi_sequence_size(),
          shape.data(),
          3
        );

        // Inference with persistent name pointers
        const char* input_names[] = { input_name_str.c_str() };
        const char* output_names[] = { output_name_str.c_str() };

        auto output = session.Run(
          Ort::RunOptions{nullptr},
          input_names,
          &input,
          1,
          output_names,
          1
        );

        // Populate Response
        float* scores = output[0].GetTensorMutableData<float>();
        for (int i = 0; i < T; ++i) {
            res->add_crisis_scores(scores[i]);
        }

        return Status::OK;

    } catch (const Ort::Exception& e) {
        LOG_CRITICAL("ONNX Runtime Failed | {}", e.what());
        return Status(grpc::StatusCode::INTERNAL, "ML Inference Failed");
    } catch (const std::exception& e) {
        LOG_CRITICAL("Std C++ Exception | {}", e.what());
        return Status(grpc::StatusCode::INTERNAL, "Service Unavailable");
    }
  }

private:
  // Static helper to configure options before session starts
  static Ort::SessionOptions CreateOptimizedOptions() {
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(1); // Restrict threads
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    return opts;
  }
   
  Ort::Env env;
  Ort::Session session;
  Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

  // FIX: Use std::string to persist the memory for the duration of the class
  std::string input_name_str;
  std::string output_name_str;
};

/**
 * @brief Entry point for the crisis score grpc service
 */
int main(int argc, char** argv) {
  logger::InitLogger("Crisis Service"); // Initialize logger

  // This ensures logs show up in 'docker logs' immediately (buffering disabled)
  std::setvbuf(stdout, NULL, _IONBF, 0);
  std::setvbuf(stderr, NULL, _IONBF, 0);

  std::string model = "/models/crisis_gru.onnx";
  if (argc > 1) model = argv[1];

  try {
    // Get port
    const char* port = std::getenv("PORT") ? std::getenv("PORT") : "50051";
    
    // Initialize service
    CrisisServiceImpl service(model);

    ServerBuilder builder;
    // Bind to 0.0.0.0 to ensure it's reachable outside the container
    builder.AddListeningPort("0.0.0.0:" + std::string(port), grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    
    if (!server) {
        LOG_CRITICAL("Service failed to start! Check if port " + std::string(port) + " is in use.");
        return 1;
    }

    LOG_INFO("Service up and listening on 0.0.0.0:" + std::string(port) + "");
    server->Wait();

  } catch (const std::exception& e) {
    // Catch initialization errors (e.g. model file not found)
    LOG_CRITICAL("Startup failed | {}", e.what());
    return 1;
  }

  return 0;
}
