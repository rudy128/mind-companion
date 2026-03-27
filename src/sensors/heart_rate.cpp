// =============================================================
// Heart Rate Sensor (MAX30102)
// Reads pulse and calculates BPM
// =============================================================
#include "heart_rate.h"
#include "../config.h"
#include "../network/logger.h"
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

static MAX30105 particleSensor;

// Internal state for BPM calculation
static const int RATE_SIZE = 10;
static float    rates[RATE_SIZE] = {0};
static int      rateSpot   = 0;
static int      rateCount  = 0;   // number of valid samples in buffer
static unsigned long lastBeat  = 0;
static float    averageBPM = 0;
static long     irValue    = 0;
static bool     fingerOn   = false;

// Used to smooth BPM  changes
static const float EMA_ALPHA = 0.35f;
// Normal heart rate range
static const float BPM_TRUST_LO = 55.0f;
static const float BPM_TRUST_HI = 120.0f;
static const float OUTLIER_RATIO = 0.45f;

// Reset all stored values
static void resetRates() {
    for (int i = 0; i < RATE_SIZE; i++) rates[i] = 0;
    rateSpot   = 0;
    rateCount  = 0;
    averageBPM = 0;
    lastBeat   = 0;
}

// Calculate average BPM from stored values
static float windowAverage() {
    if (rateCount == 0) return 0;
    float sum = 0;
    for (int i = 0; i < RATE_SIZE; i++) {
        if (rates[i] > 0) sum += rates[i];
    }
    int n = 0;
    for (int i = 0; i < RATE_SIZE; i++) if (rates[i] > 0) n++;
    return (n > 0) ? (sum / n) : 0;
}

// Initialize MAX30102 sensor
bool heartRateInit() {
    Serial.println("HR: Initializing MAX30105 — SDA=" + String(I2C_SDA) + " SCL=" + String(I2C_SCL));

    // Start sensor
    if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
        Serial.println("HR: MAX30105 not found! Check wiring.");
        Serial.println("    SDA: GPIO " + String(I2C_SDA));
        Serial.println("    SCL: GPIO " + String(I2C_SCL));
        return false;
    }

    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x1F);
    particleSensor.setPulseAmplitudeIR(0x1F);

    Serial.println("HR: MAX30105 initialized (10-sample + EMA smoothing)");
    Serial.println("HR: Place finger on sensor to see BPM...");
    return true;
}

void heartRateUpdate() {
    irValue = particleSensor.getIR();  // read IR signal

    // If finger is detected
    if (irValue > 10000) {
        // First time finger placed
        if (!fingerOn) {
            Serial.println("\nHR: Finger detected! Measuring BPM...");
            resetRates();
            fingerOn = true;
        }

        // Check if heartbeat detected
        if (checkForBeat(irValue)) {
            unsigned long delta = millis() - lastBeat;
            lastBeat = millis();

            // Reject too-short intervals 
            if (delta < 350)  return;  
            if (delta > 2500) return;   

            float bpm = 60000.0f / (float)delta;

            // Ignore invalid BPM values
            if (bpm < 20.0f || bpm > 255.0f) return;

            float winAvg = windowAverage();
            // Reject abnormal spikes
            if (bpm >= BPM_TRUST_LO && bpm <= BPM_TRUST_HI) {
                // Accept normal values
            } else if (rateCount >= 3 && winAvg > 0) {
                float lo = winAvg * (1.0f - OUTLIER_RATIO);
                float hi = winAvg * (1.0f + OUTLIER_RATIO);
                if (bpm < lo || bpm > hi) return;
            }

            // Store BPM value
            rates[rateSpot] = bpm;
            rateSpot = (rateSpot + 1) % RATE_SIZE;
            if (rateCount < RATE_SIZE) rateCount++;

            // Calculate new average
            float newAvg = windowAverage();
            if (newAvg > 0) {
                if (averageBPM > 0)
                    averageBPM = EMA_ALPHA * newAvg + (1.0f - EMA_ALPHA) * averageBPM;
                else
                    averageBPM = newAvg;
            }
        }
    } else {
        // No finger detected
        if (fingerOn) {
            Serial.println("\nHR: No finger detected");
            resetRates();
        }
        fingerOn = false;
    }
}

// Get current BPM
float heartRateGetBPM()        { return averageBPM; }

// Get raw IR value
long  heartRateGetIR()         { return irValue; }

// Check is finger is placed
bool  heartRateFingerPresent() { return fingerOn; }

// Check if BPM is abnormal
bool  heartRateIsAbnormal() {
    if (!fingerOn || averageBPM == 0) return false;
    return (averageBPM > (float)HR_ABNORMAL_HIGH || averageBPM < (float)HR_ABNORMAL_LOW);
}
