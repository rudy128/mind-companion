// =============================================================
// OpenAI API — Whisper STT + TTS
// =============================================================
#ifndef OPENAI_API_H
#define OPENAI_API_H

#include <Arduino.h>

// Send a WAV buffer to OpenAI Whisper and return the transcribed text.
// Returns empty string on failure.
String openaiTranscribe(uint8_t* wavData, size_t wavSize);

// Call OpenAI TTS API with the given text and play it immediately
// through the speaker via speakerPlayRaw(). Blocks until playback done.
// Returns true on success.
bool openaiSpeak(const char* text);

#endif // OPENAI_API_H
