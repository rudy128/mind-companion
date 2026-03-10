// =============================================================
// Heart Rate Sensor — MAX30102 (libmax3010x) Implementation
// Uses proven beat-detection logic:
//   • 100 Hz sample rate, 4× hardware averaging
//   • 411µs pulse width for best SNR
//   • LED brightness 0x2F
//   • particleSensor.check() every call, then drain FIFO
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

static void resetRates() {
    for (byte i = 0; i < RATE_SIZE; i++) rates[i] = 0;
    rateSpot   = 0;
    averageBPM = 0;
    lastBeat   = 0;
}

// ── Public API ───────────────────────────────────────────────

bool heartRateInit() {
    LOG_INFO("HR", "Initializing MAX30102...");

    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        LOG_ERROR("HR", "MAX30102 not found! Check SDA(GPIO%d) SCL(GPIO%d)", I2C_SDA, I2C_SCL);
        return false;
    }

    byte ledBrightness = 0x2F;
    byte sampleAverage = 4;
    byte ledMode       = 2;
    int  sampleRate    = 100;
    int  pulseWidth    = 411;
    int  adcRange      = 4096;

    particleSensor.setup(ledBrightness, sampleAverage, ledMode,
                         sampleRate, pulseWidth, adcRange);

    particleSensor.setPulseAmplitudeRed(0x2F);
    particleSensor.setPulseAmplitudeIR(0x2F);
    particleSensor.setPulseAmplitudeGreen(0);

    LOG_INFO("HR", "MAX30102 OK — 100Hz / 4×avg / 411us / 4096 ADC");
    return true;
}

void heartRateUpdate() {
    // Must call check() every iteration to pull new samples from
    // the sensor's I2C FIFO into the library's ring buffer.
    particleSensor.check();

    while (particleSensor.available()) {
        irValue = particleSensor.getIR();
        particleSensor.nextSample();

        if (irValue > 10000) {
            if (!fingerOn) {
                LOG_INFO("HR", "Finger detected — IR=%ld", irValue);
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
                    if (count > 0) averageBPM = sum / count;

                    LOG_INFO("HR", "BPM=%.1f  (avg of %d samples, IR=%ld)", averageBPM, count, irValue);
                }
            }
        } else {
            if (fingerOn) {
                LOG_INFO("HR", "Finger removed — IR=%ld (was %.1f bpm)", irValue, averageBPM);
                resetRates();
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
