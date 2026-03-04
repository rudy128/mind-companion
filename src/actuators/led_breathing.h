// =============================================================
// LED Breathing Pattern — 4× LEDs via simple ON/OFF toggle
// =============================================================
#ifndef LED_BREATHING_H
#define LED_BREATHING_H

#include <Arduino.h>

void ledBreathingInit();

// Non-blocking breathing: call every loop iteration while active.
// Toggles the LED every 4 seconds (ON phase = 4s, OFF phase = 4s).
// One cycle = one ON+OFF pair (8 seconds total).
// ledBreathingStart(cycles) — pass 0 for infinite, or N to auto-stop after N cycles.
void ledBreathingStart(int cycles = 3);
bool ledBreathingUpdate();   // returns true if still breathing
void ledBreathingStop();
bool ledBreathingIsActive();

#endif // LED_BREATHING_H
