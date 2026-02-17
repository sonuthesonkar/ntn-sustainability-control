cmake_minimum_required(VERSION 3.26)
project(crisis_grpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# 1. Find vcpkg packages (gRPC/Protobuf)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

# 2. Point to the Official ONNX Runtime path
set(ONNXRUNTIME_ROOT "/opt/onnxruntime")
include_directories("${ONNXRUNTIME_ROOT}/include")
link_directories("${ONNXRUNTIME_ROOT}/lib")

# 3. Executable and Generation
set(PROTO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/proto/crisis.proto")

add_executable(crisis_server
  src/server.cpp
)

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
    PLUGIN "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
)

# 4. Linking
target_include_directories(crisis_server PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(crisis_server PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    onnxruntime  # This will now find libonnxruntime.so in /opt/onnxruntime/lib
)
