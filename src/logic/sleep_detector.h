// =============================================================
// Sleep Detector — Movement + Heart Rate + RTC Analysis
// =============================================================
#ifndef SLEEP_DETECTOR_H
#define SLEEP_DETECTOR_H

#include <Arduino.h>

void   sleepDetectorInit();

// Call every loop iteration with the current movement delta.
// Internally accumulates over the configured window.
void   sleepDetectorFeed(float movementDelta);

// Current assessment
String sleepDetectorGetQuality();    // "Deep Sleep", "Light Sleep", "Restless", "Awake"
bool   sleepDetectorIsSleeping();    // true if Deep or Light sleep

#endif // SLEEP_DETECTOR_H
