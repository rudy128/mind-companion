// =============================================================
// GSR Sensor (Stress Sensor)
// Reads analog values and detects stress level.
// =============================================================
#include "gsr.h"
#include "../config.h"

// Setup GSR Sensor
void gsrInit() {
    analogReadResolution(12);
    pinMode(GSR_PIN, INPUT);
    Serial.println("[GSR] Initialized on GPIO" + String(GSR_PIN));
}

//Read sensor and calculate resistance (conductance)
   float gsrReadConductance() {
    long sum = 0;

    // Take multiple readings and average them
    for (int i = 0; i < GSR_SAMPLES; i++) {
        sum += analogRead(GSR_PIN);
    }

    float avg = sum / (float)GSR_SAMPLES;

    // Avoid divide-by-zero or invalid values
    if (avg <= 1) avg = 1;
    if (avg >= (GSR_ADC_MAX - 1)) avg = GSR_ADC_MAX - 1;

    // Resistance calculation
    float resistance = GSR_R_REF * (avg / (GSR_ADC_MAX - avg));

    return resistance;   // Lower value = more sweat
}

   
//Convert resistance value to stress level
String gsrGetStressLevel(float c) {
    // If sensor not properly worn
    if (c > GSR_WEAR_THRESHOLD)          return "Please wear finger straps";
    // Low stress
    if (c >= GSR_LOW_THRESHOLD)          return "Low";
    // Moderate stress
    if (c >= GSR_MODERATE_THRESHOLD)     return "Moderate";
    // Otherwise high stress
    return "High";
}
