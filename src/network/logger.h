// =============================================================
// Centralized Logger — M.I.N.D. Companion
// =============================================================
// Lightweight: Serial only, no RAM ring buffer.
// Use the serial debugger to view output.
// =============================================================
#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// ── Log levels ───────────────────────────────────────────────
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 3
} LogLevel;

// ── Public API ───────────────────────────────────────────────

// Call once in setup() before anything else
void logInit(LogLevel minLevel = LOG_LEVEL_DEBUG);

// Core log function (use macros below instead)
void logWrite(LogLevel level, const char* tag, const char* fmt, ...);

// ── Convenience macros ───────────────────────────────────────
#define LOG_DEBUG(tag, fmt, ...) logWrite(LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag,  fmt, ...) logWrite(LOG_LEVEL_INFO,  tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag,  fmt, ...) logWrite(LOG_LEVEL_WARN,  tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) logWrite(LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
