// =============================================================
// Command Handler — Voice Command Parsing & Action Dispatch
// =============================================================
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>

// Known commands
enum VoiceCommand {
    CMD_NONE,
    CMD_BREATHING_PATTERN,
    CMD_HOW_AM_I,
    CMD_HELP_ME,
    CMD_EMERGENCY,
    CMD_STOP
};

// Parse transcription text and return the matching command
VoiceCommand commandParse(const String& text);

// Human-readable name
String commandToString(VoiceCommand cmd);

// Execute a voice command with appropriate audio response
void commandExecute(VoiceCommand cmd);

#endif // COMMAND_HANDLER_H
