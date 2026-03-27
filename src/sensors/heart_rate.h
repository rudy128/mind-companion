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
bool   heartRateFingerPresent(); // true if IR > 10000 (finger on sensor)
bool   heartRateIsAbnormal();    // true if BPM outside HR_ABNORMAL_LOW..HR_ABNORMAL_HIGH

#endif 
