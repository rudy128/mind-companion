// =============================================================
// This file controls the Vibration Motor connected to GPIO46.
// It contains functions to start a vibration pulse for a specified duration.
// =============================================================
#include "vibration.h"
#include "../config.h"

static bool          _vibActive   = false;     // true if currently vibrating
static unsigned long _vibStartMs  = 0;        // when the current vibration started
static unsigned long _vibDuration = 0;       // duartion of the current vibration in ms

static void vibrationOn();
static void vibrationOff();

//Initializing vibration motor pin
void vibrationInit() {
    pinMode(VIBRATION_PIN, OUTPUT);
    digitalWrite(VIBRATION_PIN, LOW);
    Serial.println("[VIB] Vibration motor initialized on GPIO" + String(VIBRATION_PIN));
}

//Start a vibration pulse for the specified duration (ms)
void vibrationPulse(unsigned long durationMs) {
    vibrationOn();
    _vibActive   = true;
    _vibStartMs  = millis();
    _vibDuration = durationMs;
}

// Call in loop() to update vibration state; returns true while vibrating
bool vibrationUpdate() {
    if (!_vibActive) return false;    // not currently vibrating
    if (millis() - _vibStartMs >= _vibDuration) {
        vibrationOff();              // stop vibration after duration ended
        _vibActive = false;
        return false;
    }
    return true;                       // still vibrating
}


static void vibrationOn() {
    digitalWrite(VIBRATION_PIN, HIGH);
}

static void vibrationOff() {
    digitalWrite(VIBRATION_PIN, LOW);
}
