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

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

/**
 * @brief Implementation of the gRPC CrisisService.
 * 
 * This service handles real-time monitoring of sustainability KPIs and 
 * infers the crisis_score based on the input KPIs metrics.
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
    : env(ORT_LOGGING_LEVEL_WARNING, "crisis"),
      session(env, model_path.c_str(), Ort::SessionOptions{nullptr}) 
  {
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(1);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

    // FIX: Store the strings in std::string to own the memory
    Ort::AllocatorWithDefaultOptions allocator;
    auto in_ptr = session.GetInputNameAllocated(0, allocator);
    auto out_ptr = session.GetOutputNameAllocated(0, allocator);
    
    input_name_str = in_ptr.get();
    output_name_str = out_ptr.get();

    std::cout << "Model loaded: " << model_path 
              << " | Input: " << input_name_str 
              << " | Output: " << output_name_str << std::endl;
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
        const int T = req->seq_len();
        const int F = req->feature_dim();
        const int expected_size = T * F;

        // 1. Validation + Logging
        if (req->kpi_sequence_size() != expected_size) {
            std::cerr << "Invalid Input: Got " << req->kpi_sequence_size() 
                      << " expected " << expected_size << std::endl;
            return Status(grpc::StatusCode::INVALID_ARGUMENT, "Input size mismatch");
        }

        std::array<int64_t, 3> shape{1, (int64_t)T, (int64_t)F};

        // 2. Wrap Tensor Creation
        Ort::Value input = Ort::Value::CreateTensor<float>(
          mem,
          const_cast<float*>(req->kpi_sequence().data()),
          req->kpi_sequence_size(),
          shape.data(),
          3
        );

        // 3. Inference with persistent name pointers
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

        // 4. Populate Response
        float* scores = output[0].GetTensorMutableData<float>();
        for (int i = 0; i < T; ++i) {
            res->add_crisis_scores(scores[i]);
        }

        return Status::OK;

    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Error: " << e.what() << std::endl;
        return Status(grpc::StatusCode::INTERNAL, e.what());
    } catch (const std::exception& e) {
        std::cerr << "Std Exception: " << e.what() << std::endl;
        return Status(grpc::StatusCode::INTERNAL, "Internal C++ Server Error");
    }
  }

private:
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
  // 1. DISABLE BUFFERING: This ensures logs show up in 'docker logs' immediately
  std::setvbuf(stdout, NULL, _IONBF, 0);
  std::setvbuf(stderr, NULL, _IONBF, 0);

  std::string model = "/models/crisis_gru.onnx";
  if (argc > 1) model = argv[1];

  try {
    // 2. Initialize service
    CrisisServiceImpl service(model);

    ServerBuilder builder;
    // Bind to 0.0.0.0 to ensure it's reachable outside the container
    builder.AddListeningPort("0.0.0.0:50051",
                             grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    
    if (!server) {
        std::cerr << "Failed to start gRPC server! Check if port 50051 is in use." << std::endl;
        return 1;
    }

    std::cout << "Crisis gRPC server listening on 0.0.0.0:50051" << std::endl;
    server->Wait();

  } catch (const std::exception& e) {
    // Catch initialization errors (e.g. model file not found)
    std::cerr << "FATAL ERROR DURING STARTUP: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
