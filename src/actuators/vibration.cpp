// =============================================================
// Vibration Motor Implementation
// =============================================================
#include "vibration.h"
#include "../config.h"

void vibrationInit() {
    pinMode(VIBRATION_PIN, OUTPUT);
    digitalWrite(VIBRATION_PIN, LOW);
    Serial.println("[VIB] Vibration motor initialized on GPIO" + String(VIBRATION_PIN));
}

void vibrationPulse(unsigned long durationMs) {
    vibrationOn();
    delay(durationMs);
    vibrationOff();
}

void vibrationOn() {
    digitalWrite(VIBRATION_PIN, HIGH);
}

void vibrationOff() {
    digitalWrite(VIBRATION_PIN, LOW);
}
