/*------------------------------------------------------------------------*
 * Copyright (c) 2026 Sonu Sonkar.                                        *
 * Licensed under the MIT License.                                        *
 * See the LICENSE file in the project root for full license information. *
 *------------------------------------------------------------------------*/

/**
 * @file logger.hpp
 * @brief Standardized spdlog wrapper for the sustainability distributed mesh.
 */
#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>

namespace logger {

    /**
     * @brief Initializes the global spdlog instance for a specific service.
     * @param service_name Name to appear in the [brackets] in logs.
     */
    inline void InitLogger(const std::string& service_name) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>(service_name, console_sink);
        
        spdlog::set_default_logger(logger);
        // Format: [Timestamp] [Service] [Level] Message
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
        spdlog::set_level(spdlog::level::info);
        
        spdlog::info("{} | Logger initialized.", service_name);
    }

} // namespace logger

// Quick-access macros
#define LOG_INFO(...)  spdlog::info(__VA_ARGS__)
#define LOG_WARN(...)  spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#define LOG_CRITICAL(...) spdlog::critical(__VA_ARGS__)
