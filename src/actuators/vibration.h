// =============================================================
// Vibration Motor — Non-blocking Pulse
// =============================================================
#ifndef VIBRATION_H
#define VIBRATION_H

#include <Arduino.h>

void vibrationInit();
void vibrationPulse(unsigned long durationMs = 1000);  // starts a non-blocking pulse
bool vibrationUpdate();                                 // call every loop(); returns true while active
void vibrationOn();
void vibrationOff();

#endif // VIBRATION_H
