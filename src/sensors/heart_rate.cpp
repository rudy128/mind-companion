// =============================================================
// Heart Rate Sensor — MAX30105 Implementation
// Simple direct-read approach (no FIFO drain) — matches the
// proven standalone BPM sketch that prints to Serial.
// =============================================================
#include "heart_rate.h"
#include "../config.h"
#include "../network/logger.h"
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

static MAX30105 particleSensor;

// Rolling average — 4 samples
static const byte RATE_SIZE = 4;
static byte  rates[RATE_SIZE] = {0};
static byte  rateSpot  = 0;
static long  lastBeat  = 0;
static float averageBPM = 0;
static long  irValue    = 0;
static bool  fingerOn   = false;

// ── Helpers ──────────────────────────────────────────────────

static void resetRates() {
    for (byte i = 0; i < RATE_SIZE; i++) rates[i] = 0;
    rateSpot   = 0;
    averageBPM = 0;
    lastBeat   = 0;
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

    // Simple default setup — matches proven working BPM sketch
    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x1F);
    particleSensor.setPulseAmplitudeIR(0x1F);

    Serial.println("HR: MAX30105 initialized successfully");
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
            long delta = millis() - lastBeat;
            lastBeat = millis();

            float bpm = 60.0 / (delta / 1000.0);

            if (bpm > 20 && bpm < 255) {
                rates[rateSpot++] = (byte)bpm;
                rateSpot %= RATE_SIZE;

                float sum = 0;
                byte  count = 0;
                for (byte i = 0; i < RATE_SIZE; i++) {
                    if (rates[i] > 0) { sum += rates[i]; count++; }
                }
                if (count > 0) {
                    averageBPM = sum / count;
                    Serial.print("HR: BPM: ");
                    Serial.print(averageBPM);
                    Serial.print(" | IR: ");
                    Serial.println(irValue);
                }
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
    return (averageBPM > HR_ABNORMAL_HIGH || averageBPM < HR_ABNORMAL_LOW);
}
