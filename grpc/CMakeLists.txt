cmake_minimum_required(VERSION 3.26)
project(crisis_grpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Dependencies
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(spdlog REQUIRED)

# Find gRPC plugin binary - standard approach for system-installed gRPC
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin REQUIRED)

# 2. ONNX Runtime Configuration
# Avoid link_directories; point to the specific library for better target hygiene
set(ONNXRUNTIME_ROOT "/opt/onnxruntime" CACHE PATH "Path to ONNX Runtime root")
find_library(ONNXRUNTIME_LIB onnxruntime PATHS "${ONNXRUNTIME_ROOT}/lib" REQUIRED)

# 3. Code Generation & Executable
set(PROTO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/proto/crisis.proto")

add_executable(crisis_server src/server.cpp)

# Generate standard Protobuf files (.pb.h / .pb.cc)
protobuf_generate(
    TARGET crisis_server
    PROTOS ${PROTO_SRC}
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto"
    LANGUAGE cpp
)

# Generate gRPC files (.grpc.pb.h / .grpc.pb.cc)
protobuf_generate(
    TARGET crisis_server
    PROTOS ${PROTO_SRC}
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto"
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}"
)

# 4. Target Properties
target_include_directories(crisis_server PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/proto"
    "${ONNXRUNTIME_ROOT}/include"
)

target_link_libraries(crisis_server PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    ${ONNXRUNTIME_LIB}
    spdlog::spdlog
)
