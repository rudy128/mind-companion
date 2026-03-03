// =============================================================
// Sleep Detector — Movement + Heart Rate + RTC Analysis
// =============================================================
#ifndef SLEEP_DETECTOR_H
#define SLEEP_DETECTOR_H

#include <Arduino.h>

void   sleepDetectorInit();

// Call once per second with per-axis deltas and active-axis count.
//   dx, dy, dz   — absolute accel change per axis (m/s²)
//   activeAxes   — number of axes whose delta exceeds 0.3 m/s²
void   sleepDetectorFeed(float dx, float dy, float dz, int activeAxes);

// Current assessment
String sleepDetectorGetQuality();    // "Deep Sleep", "Light Sleep", "Restless", "Awake"
bool   sleepDetectorIsSleeping();    // true if Deep or Light sleep

#endif // SLEEP_DETECTOR_H
