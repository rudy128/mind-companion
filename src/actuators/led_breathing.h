// =============================================================
// LED Breathing Pattern — 4× LEDs via PWM
// =============================================================
#ifndef LED_BREATHING_H
#define LED_BREATHING_H

#include <Arduino.h>

void ledBreathingInit();

// Non-blocking breathing: call every loop iteration while active.
// Returns true while the breathing cycle is still running.
// A full cycle = one inhale (fade-in) + one exhale (fade-out).
// Call ledBreathingStart(cycles) to begin.
void ledBreathingStart(int cycles = 3);
bool ledBreathingUpdate();   // returns true if still breathing
void ledBreathingStop();
bool ledBreathingIsActive();

#endif // LED_BREATHING_H
