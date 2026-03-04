// =============================================================
// Vibration Motor Implementation (non-blocking)
// =============================================================
#include "vibration.h"
#include "../config.h"

static bool          _vibActive   = false;
static unsigned long _vibStartMs  = 0;
static unsigned long _vibDuration = 0;

void vibrationInit() {
    pinMode(VIBRATION_PIN, OUTPUT);
    digitalWrite(VIBRATION_PIN, LOW);
    Serial.println("[VIB] Vibration motor initialized on GPIO" + String(VIBRATION_PIN));
}

void vibrationPulse(unsigned long durationMs) {
    vibrationOn();
    _vibActive   = true;
    _vibStartMs  = millis();
    _vibDuration = durationMs;
}

bool vibrationUpdate() {
    if (!_vibActive) return false;
    if (millis() - _vibStartMs >= _vibDuration) {
        vibrationOff();
        _vibActive = false;
        return false;
    }
    return true;
}

void vibrationOn() {
    digitalWrite(VIBRATION_PIN, HIGH);
}

void vibrationOff() {
    digitalWrite(VIBRATION_PIN, LOW);
}
