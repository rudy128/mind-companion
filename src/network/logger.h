// =============================================================
// Centralized Logger — M.I.N.D. Companion
// =============================================================
// Usage:
//   LOG_INFO("HR", "Finger detected, IR=%ld", irValue);
//   LOG_WARN("GSR", "Reading near zero: %.1f", val);
//   LOG_ERROR("MPU", "Sensor not found!");
//   LOG_DEBUG("HR", "BPM=%.1f delta=%ld", bpm, delta);
//
// All entries go to Serial AND are kept in a ring buffer
// that is served at http://<ESP_IP>/api/logs
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

// ── Ring buffer size (number of log lines kept in RAM) ───────
#define LOG_BUFFER_ENTRIES  80
#define LOG_MAX_MSG_LEN     120

// ── Public API ───────────────────────────────────────────────

// Call once in setup() before anything else
void logInit(LogLevel minLevel = LOG_LEVEL_DEBUG);

// Core log function (use macros below instead)
void logWrite(LogLevel level, const char* tag, const char* fmt, ...);

// Return all buffered log entries as a JSON array string
// Caller is responsible for the returned String's lifetime
String logGetJSON();

// Clear the ring buffer
void logClear();

// Change minimum level at runtime (e.g. from dashboard)
void logSetLevel(LogLevel level);

// ── Convenience macros ───────────────────────────────────────
#define LOG_DEBUG(tag, fmt, ...) logWrite(LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag,  fmt, ...) logWrite(LOG_LEVEL_INFO,  tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag,  fmt, ...) logWrite(LOG_LEVEL_WARN,  tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) logWrite(LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
