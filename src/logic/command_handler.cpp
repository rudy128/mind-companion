// =============================================================
// This file checks what the user said in microphone input
// and decides what action the system should take.
// =============================================================
#include "command_handler.h"

VoiceCommand commandParse(const String& text) {
    String lower = text;
    lower.toLowerCase();

    if (lower.indexOf("breath") >= 0)
        return CMD_BREATHING_PATTERN;

    if (lower.indexOf("stop") >= 0 || lower.indexOf("cancel") >= 0)
        return CMD_STOP;

    return CMD_NONE;
}

String commandToString(VoiceCommand cmd) {
    switch (cmd) {
        case CMD_BREATHING_PATTERN: return "Breathing Pattern";
        case CMD_STOP:              return "Stop";
        default:                    return "None";
    }
}
