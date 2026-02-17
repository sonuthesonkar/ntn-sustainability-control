# --- STAGE 1: BUILD ---
FROM node:20-slim AS builder
WORKDIR /app
COPY web/package.json .
RUN npm install --include=dev
COPY grpc/proto ./proto
COPY web/ .
RUN npm run build

# --- STAGE 2: RUNTIME (The Lean Image) ---
FROM node:20-slim
# Metadata for Docker Hub
LABEL org.opencontainers.image.licenses="MIT"

WORKDIR /app
COPY LICENSE .

# Copy only the built app and production dependencies
COPY --from=builder /app/package.json .
COPY --from=builder /app/build ./build
COPY --from=builder /app/node_modules ./node_modules
COPY --from=builder /app/proto ./proto