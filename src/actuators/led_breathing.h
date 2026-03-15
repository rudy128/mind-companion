// =============================================================
// LED Breathing Pattern — Non-blocking linear fade (PNP inverted)
// Matches the working standalone sketch: duty 255→0 (on), 0→255 (off).
// Two trigger modes:
//   ledBreathingStart()              — indefinite (HR-triggered, dashboard cmd)
//   ledBreathingStartTimed(ms)       — auto-stops after ms milliseconds (voice)
// =============================================================
#pragma once
#include <Arduino.h>

// Initialize LEDC channel — call once in setup()
void ledBreathingInit();

// Start indefinite breathing (stopped manually via ledBreathingStop())
void ledBreathingStart();

// Start breathing that auto-stops after `durationMs` milliseconds
void ledBreathingStartTimed(unsigned long durationMs);

// Stop breathing immediately (turns LED off)
void ledBreathingStop();

// Call every loop() iteration — drives the non-blocking state machine.
// Returns true while breathing is active.
bool ledBreathingUpdate();

// True if the LED is currently breathing
bool ledBreathingIsActive();

// True if breathing was started in timed (voice) mode and has not expired yet.
// Used by main.cpp to avoid stopping a timed session when HR normalises.
bool ledBreathingIsTimedMode();