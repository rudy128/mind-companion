// =============================================================
// Sleep Detector — Movement + Heart Rate + RTC Analysis
// =============================================================
#ifndef SLEEP_DETECTOR_H
#define SLEEP_DETECTOR_H

#include <Arduino.h>

// Setup sleep detector (call once in setup)
void   sleepDetectorInit();

// Give new movement data every second
// dx, dy, dz = movement change on each axis
// activeAxes = how many axes moved above threshold
void   sleepDetectorFeed(float dx, float dy, float dz, int activeAxes);

// Get current sleep quality 
String sleepDetectorGetQuality();    // "Deep Sleep", "Light Sleep", "Awake"
bool   sleepDetectorIsSleeping();    // true if Deep or Light sleep

#endif 
