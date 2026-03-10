// =============================================================
// Command Handler Implementation
// =============================================================
#include "command_handler.h"
#include "../actuators/audio_quotes.h"
#include "../network/logger.h"

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
