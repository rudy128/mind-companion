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

   float gsrReadConductance() {
    long sum = 0;
    for (int i = 0; i < GSR_SAMPLES; i++) {
        sum += analogRead(GSR_PIN);
    }

    float avg = sum / (float)GSR_SAMPLES;

    // Prevent divide-by-zero or extreme edge cases
    if (avg <= 1) avg = 1;
    if (avg >= (GSR_ADC_MAX - 1)) avg = GSR_ADC_MAX - 1;

    // Correct resistance calculation
    float resistance = GSR_R_REF * (avg / (GSR_ADC_MAX - avg));

    return resistance;   // Lower value = more sweat
}

   

String gsrGetStressLevel(float c) {
    //if (c > GSR_HIGH_THRESHOLD)       return "High";
    //if (c >= GSR_MODERATE_LOW)        return "Moderate";
    //if (c >= GSR_LOW_THRESHOLD)       return "Low";
    //return "Please wear finger band";
    if (c > GSR_WEAR_THRESHOLD)          return "Please wear finger straps";
    if (c >= GSR_LOW_THRESHOLD)          return "Low";
    if (c >= GSR_MODERATE_THRESHOLD)     return "Moderate";
    return "High";
}
