# --- STAGE 1: BUILD ---
FROM vcpkg-base:latest AS builder

# Set up environment
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

WORKDIR /app

# Copy manifest first to leverage Docker cache for dependency builds
COPY grpc/proto ./proto
COPY grpc/ .
COPY grpc/models/ /models/

# Configure and build
# Point CMAKE_TOOLCHAIN_FILE to vcpkg's integration script
RUN cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DONNX_DISABLE_STATIC_REGISTRATION=ON \
	-DVCPKG_TARGET_TRIPLET=x64-linux \
    -DCMAKE_BUILD_TYPE=Release

RUN cmake --build build

# --- STAGE 2: RUNTIME (The Lean Image) ---
FROM ubuntu:24.04
# Metadata for Docker Hub
LABEL org.opencontainers.image.licenses="MIT"

WORKDIR /app
COPY LICENSE .

# Copy the ONNX runtime from your base (since you use LD_LIBRARY_PATH)
COPY --from=builder /opt/onnxruntime/lib /opt/onnxruntime/lib
ENV LD_LIBRARY_PATH=/opt/onnxruntime/lib

# Install minimal system libs for C++
RUN apt-get update && apt-get install -y libstdc++6 ca-certificates && rm -rf /var/lib/apt/lists/*

# Copy ONLY the final binary
COPY --from=builder /app/build/crisis_server .
COPY --from=builder /models/* ./

# Expose port and run service
EXPOSE 50051
CMD ["./crisis_server","./crisis_gru.onnx"]
