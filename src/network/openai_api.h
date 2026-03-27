// =============================================================
// OpenAI API — Speech to Text
// This file defines the function to send audio and get text back.
// =============================================================
#ifndef OPENAI_API_H
#define OPENAI_API_H

#include <Arduino.h>

// Convert recorded audio into text using OpenAI
// pcmData  = pointer to audio data
// pcmBytes = size of audio data in bytes
// returns  = converted text (empty if error)
String openaiTranscribe(int16_t* pcmData, size_t pcmBytes);

#endif 
