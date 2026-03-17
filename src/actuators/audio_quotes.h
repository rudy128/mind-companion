// =============================================================
// Audio Quotes Manager — Maps motivational quotes to audio files
// =============================================================
// Uses ESP32-audioI2S library (Audio.h) for MP3 playback.
// IMPORTANT: Call audioQuotesInit() once in setup().
// IMPORTANT: Call audioQuotesLoop() every iteration in loop().
// =============================================================
#ifndef AUDIO_QUOTES_H
#define AUDIO_QUOTES_H

#include <Arduino.h>

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

// ═══════════════════════════════════════════════════════════════
// LIFECYCLE — Call these in setup() and loop()
// ═══════════════════════════════════════════════════════════════

// Initialize the audio quotes system — call ONCE in setup()
void audioQuotesInit();

// Process audio playback — call EVERY iteration in loop()
// This is CRITICAL for audio to actually play!
void audioQuotesLoop();

// ═══════════════════════════════════════════════════════════════
// PLAYBACK FUNCTIONS
// ═══════════════════════════════════════════════════════════════

// Play a specific audio file from LittleFS (e.g., "/q1.mp3")
bool playAudioFile(const char* filepath);

// Play a file in continuous loop until audioQuotesStop() is called.
// Idempotent — calling again with the same file is a no-op.
void playFileLooped(const char* filename);

// Play a quote by looking up text in the AUDIO_QUOTE_MAP hashmap
// This is the main function to use when TFT shows a quote
bool playQuoteByHashmap(const char* quoteText);

// Play a quote struct
bool playQuote(const AudioQuote* quote);

// Play a random quote from a category
bool playQuoteByCategory(QuoteCategory category);

// ═══════════════════════════════════════════════════════════════
// TIME-SLICING — For sharing I2S with microphone
// ═══════════════════════════════════════════════════════════════

// Stop audio playback immediately (without releasing I2S)
void audioQuotesStop();

// Pause audio playback and release I2S — call BEFORE mic recording
void audioQuotesPause();

// Resume audio playback — call AFTER mic recording done
void audioQuotesResume();

// Check if audio is currently paused
bool audioQuotesIsPaused();

// ═══════════════════════════════════════════════════════════════
// QUERY FUNCTIONS
// ═══════════════════════════════════════════════════════════════

// Get a random quote from a specific category
const AudioQuote* getRandomQuote(QuoteCategory category);

// Get a specific quote by text (partial match)
const AudioQuote* getQuoteByText(const char* searchText);

// Check if LittleFS audio file exists
bool audioFileExists(const char* filepath);

// Check if audio is currently playing
bool audioQuotesIsPlaying();

// Get the text of the currently playing quote
const char* audioQuotesGetCurrentText();

// List all available quotes (for debugging)
void listAllQuotes();

// ═══════════════════════════════════════════════════════════════
// TEST FUNCTION — plays a test file to verify audio works
// ═══════════════════════════════════════════════════════════════
void audioQuotesTestPlay();

#endif // AUDIO_QUOTES_H
