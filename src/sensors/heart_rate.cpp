// =============================================================
// Heart Rate Sensor — MAX30105 Implementation
// Accurate BPM: 10-sample float rolling average, outlier rejection,
// and EMA smoothing for stable, watch-like display.
// =============================================================
#include "heart_rate.h"
#include "../config.h"
#include "../network/logger.h"
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

static MAX30105 particleSensor;

// Rolling window: more samples + float for accurate average
static const int RATE_SIZE = 10;
static float    rates[RATE_SIZE] = {0};
static int      rateSpot   = 0;
static int      rateCount  = 0;   // number of valid samples in buffer
static unsigned long lastBeat  = 0;
static float    averageBPM = 0;
static long     irValue    = 0;
static bool     fingerOn   = false;

// EMA smoothing: blend new average with previous (0.35 = responsive but smooth)
static const float EMA_ALPHA = 0.35f;
// Outlier: reject beat if its BPM is more than ±28% from current window average
static const float OUTLIER_RATIO = 0.28f;

// ── Helpers ──────────────────────────────────────────────────

static void resetRates() {
    for (int i = 0; i < RATE_SIZE; i++) rates[i] = 0;
    rateSpot   = 0;
    rateCount  = 0;
    averageBPM = 0;
    lastBeat   = 0;
}

// Current window average (only valid samples)
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

// ── Public API ───────────────────────────────────────────────

bool heartRateInit() {
    Serial.println("HR: Initializing MAX30105 — SDA=" + String(I2C_SDA) + " SCL=" + String(I2C_SCL));

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
    irValue = particleSensor.getIR();

    if (irValue > 10000) {
        if (!fingerOn) {
            Serial.println("\nHR: Finger detected! Measuring BPM...");
            resetRates();
            fingerOn = true;
        }

        if (checkForBeat(irValue)) {
            unsigned long delta = millis() - lastBeat;
            lastBeat = millis();

            // Reject too-short intervals (double trigger) and too-long (missed beat / noise)
            if (delta < 350)  return;  // > ~171 BPM single interval
            if (delta > 2500) return;   // < 24 BPM, likely missed beat

            float bpm = 60000.0f / (float)delta;

            if (bpm < 20.0f || bpm > 255.0f) return;

            float winAvg = windowAverage();
            // Outlier rejection: if we have enough history, skip beats far from current trend
            if (rateCount >= 3 && winAvg > 0) {
                float lo = winAvg * (1.0f - OUTLIER_RATIO);
                float hi = winAvg * (1.0f + OUTLIER_RATIO);
                if (bpm < lo || bpm > hi) return;
            }

            rates[rateSpot] = bpm;
            rateSpot = (rateSpot + 1) % RATE_SIZE;
            if (rateCount < RATE_SIZE) rateCount++;

            float newAvg = windowAverage();
            if (newAvg > 0) {
                if (averageBPM > 0)
                    averageBPM = EMA_ALPHA * newAvg + (1.0f - EMA_ALPHA) * averageBPM;
                else
                    averageBPM = newAvg;
            }
        }
    } else {
        if (fingerOn) {
            Serial.println("\nHR: No finger detected");
            resetRates();
        }
        fingerOn = false;
    }
}

float heartRateGetBPM()        { return averageBPM; }
long  heartRateGetIR()         { return irValue; }
bool  heartRateFingerPresent() { return fingerOn; }
bool  heartRateIsAbnormal() {
    if (!fingerOn || averageBPM == 0) return false;
    return (averageBPM > (float)HR_ABNORMAL_HIGH || averageBPM < (float)HR_ABNORMAL_LOW);
}
