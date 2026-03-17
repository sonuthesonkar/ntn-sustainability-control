# --- STAGE 1: BUILD ---
FROM node:20-slim AS builder
WORKDIR /app

# Install dependencies
COPY web/package.json web/package-lock.json ./
RUN npm ci --legacy-peer-deps

# Copy source and build
COPY grpc/proto ./proto
COPY web/ .
COPY LICENSE .

# Run Build (Vite needs all devDeps here)
RUN npm run build

# NOW prune to only production libs for the final image
RUN npm prune --production

# --- STAGE 2: RUNTIME (0 Vulnerabilities) ---
FROM gcr.io/distroless/nodejs20-debian12
# Metadata for Docker Hub / GitHub
LABEL org.opencontainers.image.description="NTN Sustainability Control - Web GUI"
LABEL org.opencontainers.image.version="${WEB_VER}-${IMAGE_TAG}-${WEB_BLD_MSG}"
LABEL org.opencontainers.image.licenses="MIT"

WORKDIR /app
ENV NODE_ENV=production

# Copy artifacts (including LICENSE)
COPY --from=builder --chown=1000:1000 /app/package.json .
COPY --from=builder --chown=1000:1000 /app/build ./build
COPY --from=builder --chown=1000:1000 /app/node_modules ./node_modules
COPY --from=builder --chown=1000:1000 /app/proto ./proto
COPY --from=builder --chown=1000:1000 /app/LICENSE . 

# Execution (Distroless runs as user 1000 by default)
USER 1000
EXPOSE 3000

# Set the binary as the Entrypoint and the script as the default Cmd
ENTRYPOINT ["/nodejs/bin/node"]
CMD ["build/index.js"]