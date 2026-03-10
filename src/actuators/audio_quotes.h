// =============================================================
// Audio Quotes Manager — Maps motivational quotes to audio files
// =============================================================
#ifndef AUDIO_QUOTES_H
#define AUDIO_QUOTES_H

#include <Arduino.h>
#include <map>

// Quote categories
enum QuoteCategory {
    QUOTE_CALM,         // Calming/relaxation quotes
    QUOTE_MOTIVATE,     // Motivational quotes
    QUOTE_BREATHE,      // Breathing exercise prompts
    QUOTE_EMERGENCY,    // Emergency situation quotes
    QUOTE_AWAKE,        // Keep awake prompts
    QUOTE_GREETING      // Greetings
};

// Audio quote entry
struct AudioQuote {
    const char* text;           // Quote text
    const char* audioFile;      // Path to audio file in LittleFS
    QuoteCategory category;     // Quote category
    uint16_t durationMs;        // Approximate duration in milliseconds
};

// Initialize the audio quotes system
void audioQuotesInit();

// Get a random quote from a specific category
const AudioQuote* getRandomQuote(QuoteCategory category);

// Get a specific quote by text (partial match)
const AudioQuote* getQuoteByText(const char* searchText);

// Play a quote (non-blocking, uses speaker system)
bool playQuote(const AudioQuote* quote);

// Play a quote by category
bool playQuoteByCategory(QuoteCategory category);

// Check if LittleFS audio file exists
bool audioFileExists(const char* filepath);

// List all available quotes (for debugging)
void listAllQuotes();

// Process audio playback loop (must be called regularly in main loop)
void audioQuotesLoop();

// Check if audio is currently playing
bool audioQuotesIsPlaying();

#endif // AUDIO_QUOTES_H
