// =============================================================
// Heart Rate Sensor (MAX30102)
// This file defines functions to read and process heart rate
// =============================================================
#ifndef HEART_RATE_H
#define HEART_RATE_H

#include <Arduino.h>

// Initialize the MAX30102 sensor on the shared I2C bus.
bool heartRateInit();

// Reads sensor data and updates BPM value
void heartRateUpdate();

float  heartRateGetBPM();        // rolling-average BPM (0 if no finger / not enough data)
long   heartRateGetIR();         // raw IR value from sensor
bool   heartRateFingerPresent(); // Check if finger is on sensor
bool   heartRateIsAbnormal();    // Check if BPM is too high or too low

#endif 
