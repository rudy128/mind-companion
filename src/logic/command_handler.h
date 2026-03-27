// =============================================================
// This file handles voice command parsing and functions used to
// detect and run them.
// =============================================================
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>

// List of all possible voice commands
enum VoiceCommand {
    CMD_NONE,
    CMD_BREATHING_PATTERN,
    CMD_HOW_AM_I,
    CMD_HELP_ME,
    CMD_EMERGENCY,
    CMD_STOP
};

// Check text and return the matched command
VoiceCommand commandParse(const String& text);

// Convert command to readable text
String commandToString(VoiceCommand cmd);

#endif
