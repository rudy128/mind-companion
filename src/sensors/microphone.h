// =============================================================
// Microphone — INMP441 I2S Recording
// =============================================================
#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <Arduino.h>

size_t micRecord(int16_t* buffer, size_t bufferSize); // records, returns bytes read

#endif 
 