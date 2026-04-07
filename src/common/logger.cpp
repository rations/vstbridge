/*
 * Copyright (C) 2026
 * VST3 Bridge - Wine VST3 Host Bridge
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "logger.h"

namespace vst3bridge {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    logLevel_ = level;
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
    if (level < logLevel_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    struct tm tm_info;
    localtime_r(&time_t, &tm_info);
    
    char time_buffer[20];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &tm_info);
    
    const char* level_str[] = {
        "[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"
    };
    
    fprintf(stderr, "[%s.%03ld] %s [%s] %s\n",
            time_buffer, ms.count(),
            level_str[static_cast<int>(level)],
            component.c_str(),
            message.c_str());
    fflush(stderr);
}

} // namespace vst3bridge