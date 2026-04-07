// =============================================================
// GSR Sensor (Stress Sensor)
// Reads analog values, converts to conductance (µS), and
// classifies stress level.
// =============================================================
#include "gsr.h"
#include "../config.h"

void gsrInit() {
    analogReadResolution(12);
    pinMode(GSR_PIN, INPUT);
    Serial.println("[GSR] Initialized on GPIO" + String(GSR_PIN));
}

float gsrReadConductance() {
    long sum = 0;

    for (int i = 0; i < GSR_SAMPLES; i++) {
        sum += analogRead(GSR_PIN);
    }

    float avg = sum / (float)GSR_SAMPLES;

    if (avg <= 1) avg = 1;
    if (avg >= (GSR_ADC_MAX - 1)) avg = GSR_ADC_MAX - 1;

    float resistance  = GSR_R_REF * (avg / (GSR_ADC_MAX - avg));
    float rawMicroS   = 1000000.0f / resistance;

    // Scale raw µS into 0–50 range: wear threshold → 0, GSR_RAW_MAX → 50
    float scaled = (rawMicroS - GSR_RAW_WEAR) / (GSR_RAW_MAX - GSR_RAW_WEAR) * 50.0f;
    if (scaled < 0.0f) scaled = 0.0f;
    if (scaled > 50.0f) scaled = 50.0f;

    return scaled;
}

String gsrGetStressLevel(float scaledValue) {
    if (scaledValue < GSR_WEAR_THRESHOLD)        return "Please wear finger straps";
    if (scaledValue <= GSR_LOW_THRESHOLD)         return "Low";
    if (scaledValue <= GSR_MODERATE_THRESHOLD)    return "Moderate";
    return "High";
}
