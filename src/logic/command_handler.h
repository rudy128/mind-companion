// =============================================================
// This file handles voice command parsing and functions used to
// detect and run them.
// =============================================================
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>

enum VoiceCommand {
    CMD_NONE,
    CMD_BREATHING_PATTERN,
    CMD_STOP
};

VoiceCommand commandParse(const String& text);

String commandToString(VoiceCommand cmd);

#endif
