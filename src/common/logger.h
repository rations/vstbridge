/*
 * Copyright (C) 2026
 * VST3 Bridge - Wine VST3 Host Bridge
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

/**
 * @file logger.h
 * @brief Thread-safe, lightweight logger for both the native library and
 *        the Wine host.
 *
 * Usage:
 *   #include "logger.h"
 *   LOG_DEBUG("process(): {} samples", numSamples);
 *   LOG_ERROR("socket send failed: {}", strerror(errno));
 *
 * All output goes to stderr.  Log level is controlled at runtime via the
 * VST3BRIDGE_LOG_LEVEL environment variable (0=none … 4=trace).
 * Default level is INFO (2).
 *
 * The macros capture __FILE__ and __LINE__ automatically; they evaluate
 * to nothing when the message level is above the current runtime level,
 * so they impose minimal overhead when disabled.
 *
 * This header is intentionally self-contained (no .cpp co-file needed)
 * so it can be included on both the Linux and Wine builds without adding
 * a shared library dependency.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <utility>

namespace vst3bridge {

// ============================================================================
// Log level enumeration
// ============================================================================

enum class LogLevel : int {
    None  = 0,   ///< Suppress all output
    Error = 1,   ///< Unrecoverable errors
    Warn  = 2,   ///< Unexpected but recoverable conditions
    Info  = 3,   ///< High-level lifecycle events (default)
    Debug = 4,   ///< Detailed per-operation tracing
    Trace = 5    ///< Very detailed (audio-thread events etc.)
};

// ============================================================================
// Logger singleton
// ============================================================================

/**
 * @brief Singleton logger with mutex-protected stderr output.
 *
 * Intentionally minimal: no file rotation, no async queue.  Both the
 * native .so and the Wine .exe are short-lived processes so simple
 * synchronous stderr output is sufficient.
 */
class Logger {
public:
    /** Get the process-global logger instance. */
    static Logger& instance() noexcept {
        static Logger inst;
        return inst;
    }

    /** Read the current log level. */
    LogLevel level() const noexcept {
        return level_.load(std::memory_order_relaxed);
    }

    /** Set the log level programmatically. */
    void setLevel(LogLevel l) noexcept {
        level_.store(l, std::memory_order_relaxed);
    }

    /**
     * @brief Emit one log line.
     *
     * Not intended to be called directly — use the LOG_* macros below.
     *
     * @param level   Log level of this message.
     * @param file    Source file name (from __FILE__).
     * @param line    Source line number.
     * @param msg     Formatted message string.
     */
    void log(LogLevel level, const char* file, int line, const std::string& msg) noexcept {
        if (level > level_.load(std::memory_order_relaxed)) {
            return;
        }

        // Timestamp: seconds.milliseconds since epoch
        auto now = std::chrono::system_clock::now();
        auto tt  = std::chrono::system_clock::to_time_t(now);
        auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch()).count() % 1000;

        char timebuf[32];
        std::tm tm_info{};
#if defined(_WIN32) && !defined(__WINE__)
        localtime_s(&tm_info, &tt);
#else
        localtime_r(&tt, &tm_info);
#endif
        std::snprintf(timebuf, sizeof(timebuf),
                      "%02d:%02d:%02d.%03d",
                      tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
                      static_cast<int>(ms));

        // Strip path from filename for brevity
        const char* basename = std::strrchr(file, '/');
        basename = (basename ? basename + 1 : file);
#ifdef _WIN32
        const char* basename2 = std::strrchr(basename, '\\');
        basename = (basename2 ? basename2 + 1 : basename);
#endif

        const char* lvlStr = levelString(level);

        // Write atomically (single fprintf call)
        std::lock_guard<std::mutex> lock(mutex_);
        std::fprintf(stderr, "[%s] [%s] %s:%d — %s\n",
                     timebuf, lvlStr, basename, line, msg.c_str());
    }

private:
    Logger() {
        // Initialise level from environment variable
        const char* env = std::getenv("VST3BRIDGE_LOG_LEVEL");
        if (env) {
            int v = std::atoi(env);
            if (v >= 0 && v <= static_cast<int>(LogLevel::Trace)) {
                level_.store(static_cast<LogLevel>(v), std::memory_order_relaxed);
            }
        }
    }

    static const char* levelString(LogLevel l) noexcept {
        switch (l) {
            case LogLevel::Error: return "ERROR";
            case LogLevel::Warn:  return "WARN ";
            case LogLevel::Info:  return "INFO ";
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Trace: return "TRACE";
            default:              return "?????";
        }
    }

    std::atomic<LogLevel> level_{LogLevel::Info};
    std::mutex            mutex_;
};

// ============================================================================
// Format helper (simple, no-dependency printf-style formatting)
// ============================================================================

namespace detail {

/**
 * @brief Variadic string formatter using snprintf.
 *
 * A *very* lightweight alternative to std::format / fmtlib.  Uses {} as
 * the placeholder (first-found replacement, not indexed), falling back
 * to the bare token if the argument cannot be converted.
 *
 * For simpler use, callers may also just pass a pre-formatted string.
 */
inline std::string logFormat(const char* fmt) {
    return std::string(fmt);
}

template<typename T, typename... Rest>
inline std::string logFormat(const char* fmt, T&& first, Rest&&... rest) {
    std::string f(fmt);
    auto pos = f.find("{}");
    if (pos == std::string::npos) {
        return f;
    }

    // Convert first argument to string
    std::string val;
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        val = first;
    } else if constexpr (std::is_same_v<std::decay_t<T>, const char*> ||
                         std::is_same_v<std::decay_t<T>, char*>) {
        val = first ? first : "(null)";
    } else if constexpr (std::is_floating_point_v<std::decay_t<T>>) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(first));
        val = buf;
    } else if constexpr (std::is_integral_v<std::decay_t<T>>) {
        val = std::to_string(first);
    } else {
        val = "?";
    }

    f.replace(pos, 2, val);
    return logFormat(f.c_str(), std::forward<Rest>(rest)...);
}

} // namespace detail

} // namespace vst3bridge

// ============================================================================
// Logging macros
// ============================================================================

/**
 * @brief Internal helper — evaluates to nothing if level is disabled.
 * @param lvl  A vst3bridge::LogLevel enum value (not a preprocessor string).
 */
#define VST3BRIDGE_LOG_IMPL(lvl, ...)                                  \
    do {                                                               \
        if (vst3bridge::Logger::instance().level() >= (lvl)) {        \
            vst3bridge::Logger::instance().log(                       \
                (lvl),                                                \
                __FILE__, __LINE__,                                   \
                vst3bridge::detail::logFormat(__VA_ARGS__));          \
        }                                                             \
    } while (false)

/** Log an error-level message. */
#define LOG_ERROR(...) VST3BRIDGE_LOG_IMPL(vst3bridge::LogLevel::Error, __VA_ARGS__)

/** Log a warning-level message. */
#define LOG_WARN(...)  VST3BRIDGE_LOG_IMPL(vst3bridge::LogLevel::Warn,  __VA_ARGS__)

/** Log an info-level message. */
#define LOG_INFO(...)  VST3BRIDGE_LOG_IMPL(vst3bridge::LogLevel::Info,  __VA_ARGS__)

/** Log a debug-level message. */
#define LOG_DEBUG(...) VST3BRIDGE_LOG_IMPL(vst3bridge::LogLevel::Debug, __VA_ARGS__)

/** Log a trace-level message (very verbose; use only in tight loops during development). */
#define LOG_TRACE(...) VST3BRIDGE_LOG_IMPL(vst3bridge::LogLevel::Trace, __VA_ARGS__)
