// =============================================================
// Microphone — INMP441 I2S Recording + WAV header
// =============================================================
#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <Arduino.h>

void   micInit();                         // configure I2S #0 in RX mode
size_t micRecord(int16_t* buffer, size_t bufferSize); // records, returns bytes read
void   micCreateWavHeader(uint8_t* wav, size_t pcmSize, uint32_t sampleRate);

#endif // MICROPHONE_H
