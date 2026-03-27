// =============================================================
// Microphone (INMP441)
// This file defines functions to record audio and create WAV data
// =============================================================
#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <Arduino.h>

void   micInit();                         // configure I2S for the INMP441 microphone
size_t micRecord(int16_t* buffer, size_t bufferSize); // records, returns bytes read
void   micCreateWavHeader(uint8_t* wav, size_t pcmSize, uint32_t sampleRate);  // Create a WAV header

#endif 
 