# --- STAGE 1: BUILD ---
FROM debian:12-slim AS builder

# 1. Install Build Tools & Core Dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    git \
    libatomic1 \
    libc-ares-dev \
    libgrpc++-dev \
    libhiredis-dev \
    libpq-dev \
    libpqxx-dev \
    libprotobuf-dev \
    libspdlog-dev \
    libssl-dev \
    nlohmann-json3-dev \
    pkg-config \
    protobuf-compiler \
    protobuf-compiler-grpc \
    wget \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# 2. Install modern cmake
RUN wget -qO- https://github.com/Kitware/CMake/releases/download/v3.28.3/cmake-3.28.3-linux-x86_64.tar.gz | tar --strip-components=1 -xz -C /usr/local

# 3. Download ONNX Runtime
WORKDIR /opt
RUN wget -q https://github.com/microsoft/onnxruntime/releases/download/v1.23.2/onnxruntime-linux-x64-1.23.2.tgz && \
    tar -xzf onnxruntime-linux-x64-1.23.2.tgz && mv onnxruntime-linux-x64-1.23.2 onnxruntime

# 4. Build All Binaries
WORKDIR /app
COPY . .
RUN cmake -B build -S ./grpc -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel $(nproc)

# Copy the models into the build folder so they are "next to" the binary
COPY ./grpc/models/* ./build/

# Create a staging area for only the middleware libs
RUN mkdir -p /app/staging_libs && \
    cp /usr/lib/x86_64-linux-gnu/*.so* /app/staging_libs/ && \
    # Manually delete the problematic system libs so they don't overwrite chainguard
    rm -f /app/staging_libs/libc.so* \
          /app/staging_libs/libstdc++.so* \
          /app/staging_libs/libm.so* \
          /app/staging_libs/libdl.so* \
          /app/staging_libs/librt.so* \
          /app/staging_libs/libpthread.so* \
          /app/staging_libs/libgcc_s.so* \
          /app/staging_libs/ld-linux-x86-64.so*

# --- CRITICAL FIX FOR STAGE 1 SYMMETRY ---
# Set WORKDIR to the build folder so Stage 1 matches Stage 2 paths
WORKDIR /app/build
# -----------------------------------------

# --- STAGE 2: RUNTIME (Chainguard) ---
# This image is built specifically to fix the vulnerabilities
FROM cgr.dev/chainguard/glibc-dynamic:latest AS runtime

# Required for versioning label
ARG GRPC_VER=0
ARG IMAGE_TAG=latest
ARG GRPC_BLD_MSG=dev
ARG BLD_NMBR=0

# Metadata for Docker Hub / GitHub
LABEL org.opencontainers.image.description="Multiple gRPC services to monitor and maintain 6G sustainability KPIs."
LABEL org.opencontainers.image.version="${GRPC_VER}-${IMAGE_TAG}-${BLD_NMBR}-${GRPC_BLD_MSG}"
LABEL org.opencontainers.image.licenses="MIT"

# Execution (nonroot distroless runs as user 65532 by default)
USER 65532

# Set WORKDIR to /app/build so ./binary works in both stages
WORKDIR /app/build

# Copy the filtered dependencies (this skips the vulnerable libc6/libssl3 from Stage 1)
COPY --from=builder --chown=65532:65532 /app/staging_libs/ /usr/lib/x86_64-linux-gnu/

# Copy onnx runtime
COPY --from=builder --chown=65532:65532 /opt/onnxruntime/lib/libonnxruntime.so* /usr/lib/x86_64-linux-gnu/

# Copy Binaries and Models into /app/build
COPY --from=builder --chown=65532:65532 /app/build/crisis_svc .
COPY --from=builder --chown=65532:65532 /app/grpc/models/* ./

COPY --from=builder --chown=65532:65532 /app/build/sustainer_svc .
COPY --from=builder --chown=65532:65532 /app/build/db_svc .
COPY --from=builder --chown=65532:65532 /app/build/ntn_svc .

# For DNS Lookup
COPY --from=builder --chown=65532:65532 /etc/nsswitch.conf /etc/nsswitch.config
ENV GRPC_DNS_RESOLVER=native

ENV LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu"

CMD [ "./sustainer_svc" ]