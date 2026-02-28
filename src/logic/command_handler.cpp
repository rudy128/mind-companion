// =============================================================
// Command Handler Implementation
// =============================================================
#include "command_handler.h"

VoiceCommand commandParse(const String& text) {
    String lower = text;
    lower.toLowerCase();

    if (lower.indexOf("breathing")  >= 0 || lower.indexOf("breathe") >= 0)
        return CMD_BREATHING_PATTERN;

    if (lower.indexOf("how am i")   >= 0 || lower.indexOf("status") >= 0)
        return CMD_HOW_AM_I;

    if (lower.indexOf("help")       >= 0 || lower.indexOf("help me") >= 0)
        return CMD_HELP_ME;

    if (lower.indexOf("emergency")  >= 0 || lower.indexOf("panic") >= 0)
        return CMD_EMERGENCY;

    if (lower.indexOf("stop")       >= 0 || lower.indexOf("cancel") >= 0)
        return CMD_STOP;

    return CMD_NONE;
}

String commandToString(VoiceCommand cmd) {
    switch (cmd) {
        case CMD_BREATHING_PATTERN: return "Breathing Pattern";
        case CMD_HOW_AM_I:          return "Status Check";
        case CMD_HELP_ME:           return "Help Request";
        case CMD_EMERGENCY:         return "Emergency";
        case CMD_STOP:              return "Stop";
        default:                    return "None";
    }
}
