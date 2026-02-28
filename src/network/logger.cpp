// =============================================================
// Centralized Logger — Implementation
// =============================================================
#include "logger.h"
#include <stdarg.h>
#include <stdio.h>

// ── Ring buffer ──────────────────────────────────────────────
struct LogEntry {
    unsigned long timestamp;   // millis()
    LogLevel      level;
    char          tag[16];
    char          msg[LOG_MAX_MSG_LEN];
};

static LogEntry  ringBuf[LOG_BUFFER_ENTRIES];
static int       head     = 0;      // next write position
static int       count    = 0;      // entries filled so far
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
    // ANSI colours for Serial monitor
    switch (l) {
        case LOG_LEVEL_DEBUG: return "\033[36m";   // cyan
        case LOG_LEVEL_INFO:  return "\033[32m";   // green
        case LOG_LEVEL_WARN:  return "\033[33m";   // yellow
        case LOG_LEVEL_ERROR: return "\033[31m";   // red
        default:              return "";
    }
}

// ── Public API ───────────────────────────────────────────────

void logInit(LogLevel level) {
    minLevel = level;
    head  = 0;
    count = 0;
    Serial.println("[LOG] Logger initialized");
}

void logSetLevel(LogLevel level) {
    minLevel = level;
}

void logWrite(LogLevel level, const char* tag, const char* fmt, ...) {
    if (level < minLevel) return;

    // Format the message
    char msgBuf[LOG_MAX_MSG_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    va_end(args);

    // Write to ring buffer
    LogEntry& e = ringBuf[head];
    e.timestamp = millis();
    e.level     = level;
    strncpy(e.tag, tag,    sizeof(e.tag) - 1);   e.tag[sizeof(e.tag)-1] = '\0';
    strncpy(e.msg, msgBuf, sizeof(e.msg) - 1);   e.msg[sizeof(e.msg)-1] = '\0';

    head = (head + 1) % LOG_BUFFER_ENTRIES;
    if (count < LOG_BUFFER_ENTRIES) count++;

    // Mirror to Serial with ANSI colour
    Serial.printf("%s[%s][%s] %lu ms — %s\033[0m\n",
                  levelColor(level),
                  levelStr(level),
                  tag,
                  e.timestamp,
                  msgBuf);
}

String logGetJSON() {
    // Walk ring buffer in chronological order
    String json = "[";

    int total   = count;                           // how many valid entries
    int startIdx = (count < LOG_BUFFER_ENTRIES)    // oldest entry index
                   ? 0
                   : head;

    for (int i = 0; i < total; i++) {
        int idx = (startIdx + i) % LOG_BUFFER_ENTRIES;
        const LogEntry& e = ringBuf[idx];

        if (i > 0) json += ",";
        json += "{";
        json += "\"t\":"   + String(e.timestamp)    + ",";
        json += "\"lvl\":\"" + String(levelStr(e.level)) + "\",";

        // Escape tag and msg for JSON safety
        String tagStr = e.tag;
        String msgStr = e.msg;
        tagStr.replace("\"", "'");
        msgStr.replace("\"", "'");
        msgStr.replace("\n", " ");

        json += "\"tag\":\"" + tagStr + "\",";
        json += "\"msg\":\"" + msgStr + "\"";
        json += "}";
    }

    json += "]";
    return json;
}

void logClear() {
    head  = 0;
    count = 0;
}
