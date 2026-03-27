// =============================================================
// Common Logger 
// This file prints messages to Serial
// =============================================================
#include "logger.h"
#include <stdarg.h>
#include <stdio.h>

static LogLevel  minLevel = LOG_LEVEL_DEBUG;

// ── Level strings ────────────────────────────────────────────
static const char* levelStr(LogLevel l) {
    switch (l) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        default:              return "?";
    }
}

static const char* levelColor(LogLevel l) {
    switch (l) {
        case LOG_LEVEL_DEBUG: return "\033[36m";   // cyan
        case LOG_LEVEL_INFO:  return "\033[32m";   // green
        case LOG_LEVEL_WARN:  return "\033[33m";   // yellow
        case LOG_LEVEL_ERROR: return "\033[31m";   // red
        default:              return "";
    }
}

// ── Public API ───────────────────────────────────────────────
//Initialize Logger
void logInit(LogLevel level) {
    minLevel = level;
    Serial.println("[LOG] Logger initialized (Serial only)");
}

void logWrite(LogLevel level, const char* tag, const char* fmt, ...) {
    if (level < minLevel) return;

    char msgBuf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    va_end(args);

    Serial.printf("%s[%s][%s] %lu ms — %s\033[0m\n",
                  levelColor(level),
                  levelStr(level),
                  tag,
                  millis(),
                  msgBuf);
}
