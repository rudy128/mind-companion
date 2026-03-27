// =============================================================
// Audio Quotes Manager — Maps motivational quotes to audio files
// =============================================================
// Uses ESP32-audioI2S library (Audio.h) for MP3 playback.
// IMPORTANT: Call audioQuotesInit() once in setup().
// Playback is driven by the audio task on Core 0 (pAudio->loop() there).
// =============================================================
#ifndef AUDIO_QUOTES_H
#define AUDIO_QUOTES_H

#include <Arduino.h>

// Initialize the audio quotes system — call ONCE in setup()
void audioQuotesInit();

// Play a file in continuous loop until audioQuotesStop() is called.
// Idempotent — calling again with the same file is a no-op.
void playFileLooped(const char* filename);

// Play a quote by looking up text in the AUDIO_QUOTE_MAP hashmap
bool playQuoteByHashmap(const char* quoteText);

// Stop audio playback immediately (without releasing I2S)
void audioQuotesStop();

// Pause audio playback and release I2S — call BEFORE mic recording
void audioQuotesPause();

// Resume audio playback — call AFTER mic recording done
void audioQuotesResume();

// Check if audio is currently playing
bool audioQuotesIsPlaying();

// Plays a test file to verify audio works
void audioQuotesTestPlay();

#endif // AUDIO_QUOTES_H
