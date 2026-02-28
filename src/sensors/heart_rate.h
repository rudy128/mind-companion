// =============================================================
// Heart Rate Sensor — MAX30102 (libmax3010x)
// =============================================================
#ifndef HEART_RATE_H
#define HEART_RATE_H

#include <Arduino.h>

// Initialize the MAX30102 sensor on the shared I2C bus.
// Wire.begin() must have been called already.
// Returns true on success.
bool heartRateInit();

// Call this every loop iteration. Reads the IR value,
// detects beats using checkForBeat(), and maintains a
// 4-sample rolling average BPM — identical to reference sketch.
void heartRateUpdate();

// Getters
float  heartRateGetBPM();        // rolling-average BPM (0 if no finger / not enough data)
long   heartRateGetIR();         // raw IR value from sensor
bool   heartRateFingerPresent(); // true if IR > 10000 (finger on sensor)
bool   heartRateIsAbnormal();    // true if BPM outside HR_ABNORMAL_LOW..HR_ABNORMAL_HIGH

#endif // HEART_RATE_H
