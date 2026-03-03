// =============================================================
// Heart Rate Sensor — MAX30102 (libmax3010x) Implementation
// Optimised for real-time, low-latency beat detection:
//   • 400 Hz sample rate, 69µs pulse width, 4096 ADC range
//   • Drain full FIFO every call (never misses a beat)
//   • I2C fast mode (400 kHz)
//   • Full state reset on finger removal → no stale data
// =============================================================
#include "heart_rate.h"
#include "../config.h"
#include "../network/logger.h"
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

static MAX30105 particleSensor;

// Rolling average — 4 samples, shows BPM after ~2 beats
static const byte RATE_SIZE = 4;
static byte  rates[RATE_SIZE] = {0};
static byte  rateSpot  = 0;
static long  lastBeat  = 0;
static float averageBPM = 0;
static long  irValue    = 0;
static bool  fingerOn   = false;

// ── Helpers ──────────────────────────────────────────────────

static void resetRollingAverage() {
    for (byte i = 0; i < RATE_SIZE; i++) rates[i] = 0;
    rateSpot   = 0;
    lastBeat   = 0;
    averageBPM = 0;
}

// ── Public API ───────────────────────────────────────────────

bool heartRateInit() {
    LOG_INFO("HR", "Initializing MAX30102...");

    // Use fast I2C (400 kHz) for lower read latency
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        LOG_ERROR("HR", "MAX30102 not found! Check SDA(GPIO%d) SCL(GPIO%d)", I2C_SDA, I2C_SCL);
        return false;
    }

    // High-performance config:
    //   sampleRate  = 400   → sensor produces a new sample every 2.5 ms
    //   pulseWidth  = 69 µs → fastest pulse width, lowest latency
    //   adcRange    = 4096  → highest sensitivity
    //   ledBright   = 0x3F  → strong enough LED for finger detection
    //   sampleAvg   = 1     → NO hardware averaging — we want every raw sample
    byte ledBrightness = 0x3F;
    byte sampleAverage = 1;
    byte ledMode       = 2;
    int  sampleRate    = 400;
    int  pulseWidth    = 69;
    int  adcRange      = 4096;

    particleSensor.setup(ledBrightness, sampleAverage, ledMode,
                         sampleRate, pulseWidth, adcRange);

    particleSensor.setPulseAmplitudeRed(0x3F);
    particleSensor.setPulseAmplitudeIR(0x3F);
    particleSensor.setPulseAmplitudeGreen(0);

    LOG_INFO("HR", "MAX30102 OK — 400Hz / 69us / 4096 ADC");
    return true;
}

void heartRateUpdate() {
    // ── Drain the entire FIFO in one call ───────────────────
    // The MAX30102 buffers up to 32 samples. If we only read
    // one per loop() call and the loop slows down (web server,
    // display, etc.) the FIFO fills with stale data and beat
    // detection lags by hundreds of milliseconds.
    // Reading every available sample here costs only a few µs
    // extra per call on fast I2C.

    byte available = particleSensor.available();
    if (available == 0) {
        particleSensor.check(); // ask sensor to fill the library FIFO
        available = particleSensor.available();
    }

    while (particleSensor.available()) {
        irValue = particleSensor.getIR();
        particleSensor.nextSample(); // advance library read pointer

        if (irValue > 10000) {
            // Finger just placed — start clean
            if (!fingerOn) {
                resetRollingAverage();
                LOG_INFO("HR", "Finger detected — IR=%ld", irValue);
            }
            fingerOn = true;

            if (checkForBeat(irValue)) {
                long delta = millis() - lastBeat;
                lastBeat = millis();
                float currentBPM = 60.0f / (delta / 1000.0f);

                LOG_DEBUG("HR", "Beat! delta=%ldms raw_BPM=%.1f", delta, currentBPM);

                // Sane physiological range
                if (currentBPM > 20 && currentBPM < 255) {
                    rates[rateSpot++] = (byte)currentBPM;
                    rateSpot %= RATE_SIZE;

                    float sum = 0;
                    byte  cnt = 0;
                    for (byte i = 0; i < RATE_SIZE; i++) {
                        if (rates[i] > 0) { sum += rates[i]; cnt++; }
                    }
                    if (cnt > 0) {
                        averageBPM = sum / cnt;
                        LOG_INFO("HR", "BPM=%.1f  (avg of %d samples, IR=%ld)", averageBPM, cnt, irValue);
                    }
                } else {
                    LOG_WARN("HR", "BPM=%.1f out of range [20-255] — discarded", currentBPM);
                }
            }
        } else {
            // Finger removed
            if (fingerOn) {
                LOG_INFO("HR", "Finger removed — IR=%ld (was %.1f bpm)", irValue, averageBPM);
                resetRollingAverage();
            }
            fingerOn = false;
        }
    }
}

float heartRateGetBPM()        { return averageBPM; }
long  heartRateGetIR()         { return irValue; }
bool  heartRateFingerPresent() { return fingerOn; }
bool  heartRateIsAbnormal() {
    if (!fingerOn || averageBPM == 0) return false;
    return (averageBPM > HR_ABNORMAL_HIGH || averageBPM < HR_ABNORMAL_LOW);
}
