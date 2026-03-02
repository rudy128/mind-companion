// =============================================================
// GSR Sensor Implementation
// =============================================================
#include "gsr.h"
#include "../config.h"

void gsrInit() {
    analogReadResolution(12);
    pinMode(GSR_PIN, INPUT);
    Serial.println("[GSR] Initialized on GPIO" + String(GSR_PIN));
}

float gsrReadResistance() {
    long sum = 0;
    for (int i = 0; i < GSR_SAMPLES; i++) {
        sum += analogRead(GSR_PIN);
        // Removed delay(5) — no blocking in the main loop.
        // The ADC settles in < 10 µs at 12-bit; no delay needed.
    }
    float avg = sum / (float)GSR_SAMPLES;
    if (avg < 1) avg = 1; // prevent division by zero
    return GSR_R_REF * (GSR_ADC_MAX / avg - 1.0f);
}

String gsrGetStressLevel(float r) {
    if (r > GSR_HIGH_THRESHOLD)       return "High";
    if (r >= GSR_MODERATE_LOW)        return "Moderate";
    if (r >= GSR_LOW_THRESHOLD)       return "Low";
    return "Please wear finger band";
}
