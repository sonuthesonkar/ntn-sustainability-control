# --- STAGE 1: BUILD ---
FROM debian:12-slim AS builder

# Install Build Tools & Dev Dependencies
# Combined into one RUN to reduce layer overhead
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    libc-ares-dev \
    libgrpc++-dev \
    libprotobuf-dev \
    libre2-dev \
    libspdlog-dev \
    libssl-dev \
    nlohmann-json3-dev \
    pkg-config protobuf-compiler \
    protobuf-compiler-grpc \
    wget \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Install modern cmake
RUN wget -qO- https://github.com/Kitware/CMake/releases/download/v3.28.3/cmake-3.28.3-linux-x86_64.tar.gz | \
    tar --strip-components=1 -xz -C /usr/local

# Download ONNX Runtime
WORKDIR /opt
RUN wget -q https://github.com/microsoft/onnxruntime/releases/download/v1.23.2/onnxruntime-linux-x64-1.23.2.tgz && \
    tar -xzf onnxruntime-linux-x64-1.23.2.tgz && mv onnxruntime-linux-x64-1.23.2 onnxruntime

# Build App
WORKDIR /app
COPY grpc/ ./grpc/
RUN cmake -B build -S ./grpc -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel $(nproc)

# --- STAGE 2: RUNTIME (0 Vulnerabilities) ---
FROM gcr.io/distroless/cc-debian12
# Required for versioning label
ARG GRPC_VER=0
ARG IMAGE_TAG=latest
ARG GRPC_BLD_MSG=dev

# Metadata for Docker Hub / GitHub
LABEL org.opencontainers.image.description="gRPC Service to infer crisis score from sustainability KPIs"
LABEL org.opencontainers.image.version="${GRPC_VER}-${IMAGE_TAG}-${GRPC_BLD_MSG}"
LABEL org.opencontainers.image.licenses="MIT"

WORKDIR /app

# Copy ALL dependencies (found by ldd)
# Using wildcards to catch all the libabsl, libgrpc, libssl, etc.
COPY --from=builder /usr/lib/x86_64-linux-gnu/libgrpc*.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libprotobuf.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libspdlog.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libabsl_*.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libfmt.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libupb.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libcrypto.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libssl.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libcares.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libaddress_sorting.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libre2.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libgpr.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /lib/x86_64-linux-gnu/libz.so* /lib/x86_64-linux-gnu/
# FIX: Copy libonnxruntime directly into the main library path - required
COPY --from=builder /opt/onnxruntime/lib/libonnxruntime.so* /usr/lib/x86_64-linux-gnu/


# Copy App
COPY --from=builder /app/build/crisis_server .
COPY --from=builder /app/grpc/models/* ./

# Simplified LD_LIBRARY_PATH as everything is now in standard locations
ENV LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu"

# Open port
EXPOSE 50051

# Launch
ENTRYPOINT ["./crisis_server", "./crisis_gru.onnx"]
