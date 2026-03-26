/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

/**
 * @file websocket_server.js
 * @brief Gateway bridging redis 'UPDATE_GUI' to websockets.
 */

import { handler } from './build/handler.js'; 
import express from 'express';
import http from 'http';
import { WebSocketServer, WebSocket } from 'ws';
import { createClient } from 'redis';
import { createLogger, format, transports } from 'winston';

const { combine, timestamp, printf, colorize } = format;

const logFormat = printf(({ timestamp, level, message, service, ...meta }) => {
  // Capture extra metadata (like your IP object) if it exists
  const extra = Object.keys(meta).length ? ` ${JSON.stringify(meta)}` : '';
  
  // Matches: [Timestamp] [Service] [Level] Message
  return `[${timestamp}] [${service}] [${level}] ${message}${extra}`;
});

// Winston logger
const wsLogger = createLogger({
  level: process.env.LOG_LEVEL || 'info',
  format: combine(
    timestamp({ format: 'YYYY-MM-DD HH:mm:ss.SSS' }), 
    colorize({ all: true }), // This maps to [%^...%$] for colors
    logFormat
  ),
  defaultMeta: { service: 'web-socket' },
  transports: [new transports.Console()]
});

const app = express();
const server = http.createServer(app);

// Web socket server
const wss = new WebSocketServer({ 
  server, 
  path: '/ws/state',
  clientTracking: true 
});

wss.on('connection', (ws, req) => {
  const ip = req.socket.remoteAddress;
  wsLogger.debug('WebSocket connection established', { ip });

  ws.on('error', (err) => wsLogger.error('WebSocket client error', { message: err.message }));
  ws.on('close', () => wsLogger.debug('WebSocket client disconnected'));
});

// Redis mesh bridge
const REDIS_URL = process.env.REDIS_URL || 'redis://redis:6379';
const subscriber = createClient({ url: REDIS_URL });

subscriber.on('error', (err) => {
  wsLogger.error('Redis subscriber error', { error: err.message });
});

/**
 * @brief Initializes redis subscription and handles the broadcast logic.
 */
async function startMeshListener() {
  try {
    await subscriber.connect();
    wsLogger.info('Gateway connected to redis mesh event bus');

    // Sub: Notify
    await subscriber.subscribe('UPDATE_GUI', (message) => {
      wsLogger.debug('Received UPDATE_GUI from mesh, pushing to sockets');

      // Refresh GUI (broadcast)
      let count = 0;
      wss.clients.forEach((client) => {
        if (client.readyState === WebSocket.OPEN) {
          client.send(message);
          count++;
        }
      });
      wsLogger.debug('Mesh state broadcast complete', { notified_clients: count });
    });
  } catch (err) {
    wsLogger.error('Mesh bridge initialization failed', { error: err.message });
    process.exit(1);
  }
}

// Execute background bridge
startMeshListener();

// SvelteKit middleware
// All web traffic handled by SvelteKit build
app.use(handler);

// Server lifecycle
const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
  wsLogger.info('NTN Sustainability Control Gateway listening', { port: PORT, env: process.env.NODE_ENV });
});

/**
 * @brief Graceful shutdown for Docker SIGTERM/SIGINT.
 */
const shutdown = async () => {
  wsLogger.info('Shutting down gateway...');
  await subscriber.quit();
  server.close(() => {
    wsLogger.info('Server process terminated');
    process.exit(0);
  });
};

process.on('SIGTERM', shutdown);
process.on('SIGINT', shutdown);
