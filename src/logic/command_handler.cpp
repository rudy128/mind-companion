// =============================================================
// This file checks what the user said in microphone input 
// and decides what action the system should take.
// =============================================================
#include "command_handler.h"
#include "../actuators/audio_quotes.h"
#include "../network/logger.h"

//Convert voice text into a command
VoiceCommand commandParse(const String& text) {
    String lower = text;
    lower.toLowerCase();

    
    // Check if user is asking about breathing , such as;
    //   "breathing pattern", "help me breathe", "help with breathing",
    //   "i need breathing pattern", "breathe", etc.
    if (lower.indexOf("breath") >= 0)
        return CMD_BREATHING_PATTERN;

    if (lower.indexOf("how am i")   >= 0 || lower.indexOf("status") >= 0)
        return CMD_HOW_AM_I;

    // Only match plain "help" as CMD_HELP_ME if no breathing keyword present
    // ("help me breathe" is already caught above)
    if (lower.indexOf("help")      >= 0 || lower.indexOf("help me") >= 0)
        return CMD_HELP_ME;

    if (lower.indexOf("emergency") >= 0 || lower.indexOf("panic") >= 0)
        return CMD_EMERGENCY;

    if (lower.indexOf("stop")      >= 0 || lower.indexOf("cancel") >= 0)
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

//Run action based on detected command
void commandExecute(VoiceCommand cmd) {
    LOG_INFO("CMD", "Executing command: %s", commandToString(cmd).c_str());

    switch (cmd) {
        case CMD_BREATHING_PATTERN:
            LOG_INFO("CMD", "Starting breathing pattern");
            playQuoteByCategory(QUOTE_BREATHE);
            // TODO: Trigger LED breathing pattern
            // TODO: Update TFT display with breathing instructions
            break;

        case CMD_HOW_AM_I:
            LOG_INFO("CMD", "Status check requested");
            // Play a calm reassurance quote
            playQuoteByCategory(QUOTE_CALM);
            // TODO: Display current vitals on TFT
            break;

        case CMD_HELP_ME:
            LOG_INFO("CMD", "Help requested");
            playQuoteByCategory(QUOTE_CALM);
            // TODO: Increase monitoring sensitivity
            // TODO: Offer to contact emergency contacts
            break;

        case CMD_EMERGENCY:
            LOG_INFO("CMD", "EMERGENCY activated");
            playQuoteByCategory(QUOTE_EMERGENCY);
            // TODO: Trigger emergency protocol
            // TODO: Activate alarm/buzzer
            // TODO: Send emergency notifications
            break;

        case CMD_STOP:
            LOG_INFO("CMD", "Stop command received");
            // Stop any active alarms, breathing patterns, etc.
            // TODO: Stop alarm
            // TODO: Stop breathing LED pattern
            break;

        case CMD_NONE:
        default:
            LOG_WARN("CMD", "Unknown or no command");
            break;
    }
}
