/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

/**
 * @file mesh_store.ts
 * @brief WebSocket signal bridge for web browser dashboards.
 */

import { writable } from 'svelte/store';
import { browser } from '$app/environment';

export const meshPulse = writable<number>(0);
export const isConnected = writable<boolean>(false);
/**
 * @brief Global store for mesh-related error messages. 
 * Linked to the dashboard's UI error label.
 */
export const meshError = writable<string | null>(null);

export function initMeshSync() {
    if (!browser) return;

    let socket: WebSocket | null = null;
    let reconnectTimer: ReturnType<typeof setTimeout>;
    let retryCount = 0;
    const MAX_RETRIES = 5;

    const connect = () => {
        try {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const endpoint = `${protocol}//${window.location.host}/ws/state`;
            
            socket = new WebSocket(endpoint);

            socket.onopen = () => {
                isConnected.set(true);
                meshError.set(null); // Clear any previous errors on successful sync
                retryCount = 0;
            };

            socket.onmessage = (event: MessageEvent) => {
                if (event.data === "state_changed") {
                    meshPulse.update(n => n + 1);
                }
            };

            socket.onclose = (event: CloseEvent) => {
                isConnected.set(false);
                if (!event.wasClean && retryCount < MAX_RETRIES) {
                    retryCount++;
                    meshError.set(`Sync lost. Retrying (${retryCount}/${MAX_RETRIES})...`);
                    reconnectTimer = setTimeout(connect, 3000);
                } else if (retryCount >= MAX_RETRIES) {
                    meshError.set("Mesh Gateway Unreachable. Check connection.");
                }
            };

            socket.onerror = () => {
                // Triggered on protocol failure (e.g. 404/500 on the WS upgrade)
                meshError.set("WebSocket Protocol Error");
                socket?.close();
            };

        } catch (err: any) {
            meshError.set("Critical: Sync Initialization Failed");
            throw err; // Bubble up so it hits SvelteKit's error boundary/logging
        }
    };

    connect();

    return () => {
        clearTimeout(reconnectTimer);
        if (socket) {
            socket.onclose = null; 
            socket.close();
        }
    };
}
