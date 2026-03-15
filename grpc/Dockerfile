# --- STAGE 1: BUILD ---
FROM ubuntu:24.04 AS builder

# 1.1 Install Build Tools & Dev Dependencies
# Combined into one RUN to reduce layer overhead
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ca-certificates \
    libgrpc++-dev \
    libprotobuf-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    pkg-config \
    protobuf-compiler \
    protobuf-compiler-grpc \
    wget \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# 1.2 Download ONNX Runtime (v1.23.2)
WORKDIR /opt
RUN wget -q https://github.com/microsoft/onnxruntime/releases/download/v1.23.2/onnxruntime-linux-x64-1.23.2.tgz && \
    tar -xzf onnxruntime-linux-x64-1.23.2.tgz && \
    mv onnxruntime-linux-x64-1.23.2 onnxruntime && \
    rm onnxruntime-linux-x64-1.23.2.tgz

# 1.3 Build the Application
WORKDIR /app
COPY grpc/ ./grpc/

# Ensure build directory is clean and compile
# Note: Pointing to /opt/onnxruntime is handled in CMakeLists.txt
RUN cmake -B build -S ./grpc \
    -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel $(nproc)

# --- STAGE 2: RUNTIME (The Lean Image) ---
FROM ubuntu:24.04
# Metadata for Docker Hub
LABEL org.opencontainers.image.licenses="MIT"

WORKDIR /app
COPY LICENSE .
# 2.1 Install specific Shared Libraries (Noble versions)
# t64 suffix is required for Ubuntu 24.04 (Noble)
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgrpc++1.51t64 \
    libprotobuf32t64 \
    libspdlog1.12 \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# 2.2 Copy dependencies and binary from builder
COPY --from=builder /opt/onnxruntime/lib /opt/onnxruntime/lib
COPY --from=builder /app/build/crisis_server .
COPY --from=builder /app/grpc/models/* ./

# 2.3 Environment & Execution
# Ensure the dynamic loader finds libonnxruntime.so
ENV LD_LIBRARY_PATH="/opt/onnxruntime/lib"

EXPOSE 50051

# Use JSON syntax for CMD to handle signals (SIGTERM) correctly
ENTRYPOINT ["./crisis_server"]
CMD ["./crisis_gru.onnx"]
