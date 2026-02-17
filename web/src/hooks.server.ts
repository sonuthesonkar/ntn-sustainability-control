/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/
import { logger, getErrorLocation } from '$lib/logger.server'; // Your Winston instance
import type { HandleServerError } from '@sveltejs/kit';

/**
 * @brief Generic error handler
 * @param param0 - error, event
 * @returns message and code hinting internal error
 */
export const handleError: HandleServerError = ({ error, event }) => {
    const err = error as Error;

    // Log the full error to Winston (Docker captures this)
    logger.error('Unexpected server error', {
        message: err.message || 'Internal error',
        route: event.route.id,
        url: event.url.pathname,
        location: getErrorLocation(err),
        stack: err.stack
    });

    // Return what the user sees (avoids leaking stack traces)
    return {
        message: 'A server-side error occurred.',
        code: 'INTERNAL_ERROR'
    };
};
