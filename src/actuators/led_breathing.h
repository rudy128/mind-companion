// =============================================================
// Handles the LED fade pattern.The functions below control
// initialization, start/stop behavior, and state updates.
// =============================================================
#pragma once
#include <Arduino.h>

// Initialize LEDC channel — call once in setup()
void ledBreathingInit();

// Start continuous repeating fade (runs until stopped)
void ledBreathingStart();

// Stop immediately and turn LED off
void ledBreathingStop();

// Smooth LED Stop — finishes the current fade-out then turns off.
// If mid fade-in, reverses direction first so the LED dims smoothly.
void ledBreathingGracefulStop();

// Updates the LED state and returns true while breathing is active
bool ledBreathingUpdate();

// True if the LED is currently breathing
bool ledBreathingIsActive();