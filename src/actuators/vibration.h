// =============================================================
// Vibration Motor — Hourly Reminder
// =============================================================
#ifndef VIBRATION_H
#define VIBRATION_H

#include <Arduino.h>

void vibrationInit();
void vibrationPulse(unsigned long durationMs = 1000);  // blocking pulse
void vibrationOn();
void vibrationOff();

#endif // VIBRATION_H
