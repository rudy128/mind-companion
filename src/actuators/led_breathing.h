// =============================================================
// LED Breathing Pattern — Non-blocking linear fade (PNP inverted)
// Worker only — timer/duration is managed by the caller (main.cpp).
// =============================================================
#pragma once
#include <Arduino.h>

// Initialize LEDC channel — call once in setup()
void ledBreathingInit();

// Start continuous repeating fade (runs until stopped)
void ledBreathingStart();

// Stop immediately and turn LED off
void ledBreathingStop();

// Graceful stop — finishes the current fade-out then turns off.
// If mid fade-in, reverses direction first so the LED dims smoothly.
void ledBreathingGracefulStop();

// Drive the state machine — call every loop() iteration
// Returns true while breathing is active
bool ledBreathingUpdate();

// True if the LED is currently breathing
bool ledBreathingIsActive();