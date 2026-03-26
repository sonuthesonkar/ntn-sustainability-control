# --- STAGE 1: BUILD ---
FROM node:20-slim AS builder
WORKDIR /app

# 1. Install dependencies (including devDeps for Vite build)
COPY web/package.json web/package-lock.json ./
RUN npm ci --legacy-peer-deps

# 2. Copy source
COPY grpc/proto ./proto
COPY web/ .
COPY LICENSE .

# 3. Build SvelteKit (outputs to /app/build)
RUN npm run build

# 4. Prune to only production-grade libraries
# CRITICAL: express, ws, redis, and winston MUST be in 'dependencies'
RUN npm prune --production

# --- STAGE 2: RUNTIME ---
FROM gcr.io/distroless/nodejs20-debian12:nonroot AS runtime
# Required for versioning label
ARG WEB_VER=0
ARG IMAGE_TAG=latest
ARG WEB_BLD_MSG=dev

# Metadata for Docker Hub / GitHub
LABEL org.opencontainers.image.description="NTN Sustainability Control - Web GUI"
LABEL org.opencontainers.image.version="${WEB_VER}-${IMAGE_TAG}-${WEB_BLD_MSG}"
LABEL org.opencontainers.image.licenses="MIT"

# Execution (nonroot distroless runs as user 65532 by default)
USER 65532

WORKDIR /app
ENV NODE_ENV=production

# Copy Production Artifacts
COPY --from=builder --chown=65532:65532 /app/package.json .
COPY --from=builder --chown=65532:65532 /app/build ./build
COPY --from=builder --chown=65532:65532 /app/node_modules ./node_modules
COPY --from=builder --chown=65532:65532 /app/proto ./proto
COPY --from=builder --chown=65532:65532 /app/LICENSE . 
# Ensure this file exists in /web/ before building
COPY --from=builder --chown=65532:65532 /app/websocket_server.js .

EXPOSE 3000

# Set entrypoint to our custom WebSocket/Redis bridge
ENTRYPOINT ["/nodejs/bin/node"]
CMD ["websocket_server.js"]
