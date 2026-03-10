// =============================================================
// OpenAI API — Whisper STT (Speech-to-Text)
// =============================================================
#ifndef OPENAI_API_H
#define OPENAI_API_H

#include <Arduino.h>

// Send PCM audio to OpenAI Whisper and return the transcribed text.
// Streams the upload directly — no extra buffer copy needed.
// pcmData: pointer to raw 16-bit mono PCM samples (the audioBuffer)
// pcmBytes: size in bytes of the PCM data
// Returns empty string on failure.
String openaiTranscribe(int16_t* pcmData, size_t pcmBytes);

#endif // OPENAI_API_H
