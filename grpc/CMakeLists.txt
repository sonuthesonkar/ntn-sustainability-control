cmake_minimum_required(VERSION 3.26)
project(ntn_mesh LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Dependencies
find_package(nlohmann_json REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(spdlog REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# Dependency discovery
pkg_check_modules(PQXX REQUIRED libpqxx)
pkg_check_modules(POSTGRES REQUIRED libpq)
pkg_check_modules(HIREDIS REQUIRED hiredis)

find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin REQUIRED)

# 2. ONNX Runtime Configuration
set(ONNXRUNTIME_ROOT "/opt/onnxruntime" CACHE PATH "Path to ONNX Runtime root")
find_library(ONNXRUNTIME_LIB onnxruntime PATHS "${ONNXRUNTIME_ROOT}/lib" REQUIRED)

# 3. Proto Generation
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTOS 
    "${PROTO_DIR}/crisis.proto" 
    "${PROTO_DIR}/db.proto" 
    "${PROTO_DIR}/ntn.proto" 
    "${PROTO_DIR}/sustainer.proto"
)

protobuf_generate(
    PROTOS ${PROTOS}
    IMPORT_DIRS ${PROTO_DIR}
    LANGUAGE cpp
    OUT_VAR PROTO_SRCS
)
protobuf_generate(
    PROTOS ${PROTOS}
    IMPORT_DIRS ${PROTO_DIR}
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}"
    OUT_VAR GRPC_SRCS
)

# 4. Binaries
# Crisis Inference Server
add_executable(crisis_svc src/crisis.cpp ${PROTO_SRCS} ${GRPC_SRCS})
target_link_libraries(crisis_svc PRIVATE gRPC::grpc++ protobuf::libprotobuf ${ONNXRUNTIME_LIB} spdlog::spdlog)

# Sustainability Orchestrator
add_executable(sustainer_svc src/sustainer.cpp ${PROTO_SRCS} ${GRPC_SRCS})
target_link_libraries(sustainer_svc PRIVATE 
    gRPC::grpc++ 
    protobuf::libprotobuf 
    ${HIREDIS_LIBRARIES} 
    spdlog::spdlog 
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# NTN state machine
add_executable(ntn_svc src/ntn.cpp ${PROTO_SRCS} ${GRPC_SRCS})
target_link_libraries(ntn_svc PRIVATE gRPC::grpc++ protobuf::libprotobuf spdlog::spdlog)

# DB Service
add_executable(db_svc src/db.cpp ${PROTO_SRCS} ${GRPC_SRCS})
target_link_libraries(db_svc PRIVATE 
    gRPC::grpc++ 
    protobuf::libprotobuf 
    ${PQXX_LIBRARIES} 
    ${POSTGRES_LIBRARIES} 
    ${HIREDIS_LIBRARIES} 
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# 5. Include Required Paths
# Ensure db_service is in the loop to inherit paths
foreach(TGT crisis_svc sustainer_svc ntn_svc db_svc)
    target_include_directories(${TGT} PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/src" 
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/proto" 
        "${ONNXRUNTIME_ROOT}/include"
        ${HIREDIS_INCLUDE_DIRS}
        ${PQXX_INCLUDE_DIRS}
        ${PostgreSQL_INCLUDE_DIRS}
    )
endforeach()
