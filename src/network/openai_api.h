// =============================================================
// OpenAI API — Whisper STT
// =============================================================
#ifndef OPENAI_API_H
#define OPENAI_API_H

#include <Arduino.h>

// Send a WAV buffer to OpenAI Whisper and return the transcribed text.
// Returns empty string on failure.
String openaiTranscribe(uint8_t* wavData, size_t wavSize);

#endif // OPENAI_API_H
