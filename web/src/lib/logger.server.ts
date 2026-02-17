/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/
import winston from 'winston';

/**
 * @brief Helper function - get location of the error.
 * @param error 
 * @returns 
 */
export function getErrorLocation(error: Error): string {
  if (!error.stack) return 'Unknown location';

  const stackLines = error.stack.split('\n');
  // Usually, index 1 or 2 contains the caller location
  // Look for the first line that isn't the logger file itself
  const callerLine = stackLines.find(line => 
    line.includes('/') && !line.includes('logger.server.ts')
  );

  if (!callerLine) return 'Location not found';

  // Regex to extract file name, line, and column
  const match = callerLine.match(/([^/\\()]+:\d+:\d+)/);
  return match ? match[0] : callerLine.trim();
}

/**
 * Create the winston instance
 */
export const logger = winston.createLogger({
    level: 'error',
    format: winston.format.combine(
        winston.format.errors({ stack: true }),
        winston.format.timestamp(),
        winston.format.json() // JSON is easier for Docker/Cloud logs to parse
    ),
    transports: [
        // Write to the console (Docker will capture this automatically)
        new winston.transports.Console({
            format: winston.format.combine(
                winston.format.colorize(),
                winston.format.simple()
            )
        })
    ]
});

/**
 * @brief Enhanced helper that works with the winston format.
 */
export const logError = (error: Error, context: string = 'General') => {
  logger.error(error.message, { // Important - 1st argument here should be a string.
    // Passing the error object itself as 'error'
    // Winston's { stack: true } format will expand this
    error, 
    context,
    location: getErrorLocation(error), // custom localized info
  });
};
