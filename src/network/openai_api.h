// =============================================================
// OpenAI API — Whisper STT + TTS
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

// Call OpenAI TTS API with the given text and play it immediately
// through the speaker via speakerPlayRaw(). Blocks until playback done.
// Returns true on success.
bool openaiSpeak(const char* text);

#endif // OPENAI_API_H
