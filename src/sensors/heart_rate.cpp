// =============================================================
// Heart Rate Sensor (MAX30102)
// Reads pulse and calculates BPM
// =============================================================
#include "heart_rate.h"
#include "../config.h"
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
static bool       beatTimingArmed = false;  // first beat after reset only arms RR clock
static float      averageBPM = 0;
static long       irValue    = 0;
static bool       fingerOn   = false;

// Used to smooth BPM  changes
static const float EMA_ALPHA = 0.35f;
// Normal heart rate range (tight gate once we have a stable window)
static const float BPM_TRUST_LO = 55.0f;
static const float BPM_TRUST_HI = 120.0f;
// While still converging, accept a wider band so early beats populate the window
static const float BPM_WARM_LO  = 40.0f;
static const float BPM_WARM_HI  = 200.0f;
static const float OUTLIER_RATIO = 0.45f;

// Reset all stored values
static void resetRates() {
    for (int i = 0; i < RATE_SIZE; i++) {
        rates[i] = 0;
    }
    rateSpot          = 0;
    rateCount         = 0;
    averageBPM        = 0;
    lastBeat          = 0;
    beatTimingArmed   = false;
}

// Calculate average BPM from stored values
static float windowAverage() {
    if (rateCount == 0) {
        return 0;
    }
    float sum = 0;
    for (int i = 0; i < RATE_SIZE; i++) {
        if (rates[i] > 0) {
            sum += rates[i];
        }
    }
    int n = 0;
    for (int i = 0; i < RATE_SIZE; i++) {
        if (rates[i] > 0) {
            n++;
        }
    }
    return (n > 0) ? (sum / n) : 0;
}

// One IR sample's beat logic (never aborts the outer FIFO drain).
static bool sawStrongIrThisUpdate = false;

static void tryProcessBeat(long ir) {
    if (!checkForBeat(ir)) {
        return;
    }

    const unsigned long now = millis();
    if (!beatTimingArmed) {
        lastBeat        = now;
        beatTimingArmed = true;
        return;
    }

    unsigned long delta = now - lastBeat;
    lastBeat            = now;

    if (delta < 350UL || delta > 2500UL) {
        return;
    }

    float bpm = 60000.0f / (float)delta;

    if (bpm < 20.0f || bpm > 255.0f) {
        return;
    }

    const float winAvg = windowAverage();
    const bool  warming  = (rateCount < 5);

    const float trustLo = warming ? BPM_WARM_LO : BPM_TRUST_LO;
    const float trustHi = warming ? BPM_WARM_HI : BPM_TRUST_HI;

    if (bpm >= trustLo && bpm <= trustHi) {
        // accept
    } else if (rateCount >= 3 && winAvg > 0) {
        const float lo = winAvg * (1.0f - OUTLIER_RATIO);
        const float hi = winAvg * (1.0f + OUTLIER_RATIO);
        if (bpm < lo || bpm > hi) {
            return;
        }
    } else {
        return;
    }

    rates[rateSpot] = bpm;
    rateSpot        = (rateSpot + 1) % RATE_SIZE;
    if (rateCount < RATE_SIZE) {
        rateCount++;
    }

    const float newAvg = windowAverage();
    if (newAvg > 0) {
        if (averageBPM > 0) {
            averageBPM = EMA_ALPHA * newAvg + (1.0f - EMA_ALPHA) * averageBPM;
        } else {
            averageBPM = newAvg;
        }
    }
}

static void processIrSample(long ir) {
    irValue = ir;
    if (ir <= (long)HR_FINGER_IR_MIN) {
        return;
    }
    sawStrongIrThisUpdate = true;
    if (!fingerOn) {
        Serial.println("\nHR: Finger detected! Measuring BPM...");
        resetRates();
        fingerOn = true;
    }
    tryProcessBeat(ir);
}

// Initialize MAX30102 sensor
bool heartRateInit() {
    Serial.println("HR: Initializing MAX30105 — SDA=" + String(I2C_SDA) + " SCL=" + String(I2C_SCL));

    if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
        Serial.println("HR: MAX30105 not found! Check wiring.");
        Serial.println("    SDA: GPIO " + String(I2C_SDA));
        Serial.println("    SCL: GPIO " + String(I2C_SCL));
        return false;
    }

    // Explicit setup: higher sample rate + FIFO averaging helps checkForBeat()
    particleSensor.setup(
        HR_LED_BRIGHTNESS,
        HR_SAMPLE_AVERAGE,
        2,                      // Red + IR
        HR_SAMPLE_RATE_HZ,
        411,                    // pulse width (µs-ish slot per SparkFun table)
        4096);                  // ADC range
    particleSensor.setPulseAmplitudeRed(0);   // IR-only path reduces cross-talk for many boards
    particleSensor.setPulseAmplitudeIR(HR_LED_BRIGHTNESS);

    Serial.println("HR: MAX30105 initialized (FIFO drain + beat timing + EMA)");
    Serial.println("HR: Place finger on sensor to see BPM...");
    return true;
}

void heartRateUpdate() {
    particleSensor.check();

    sawStrongIrThisUpdate = false;

    if (particleSensor.available()) {
        while (particleSensor.available()) {
            processIrSample(particleSensor.getIR());
        }
    } else {
        // FIFO empty this pass — still sample once or finger/BPM stall
        processIrSample(particleSensor.getIR());
    }

    if (!sawStrongIrThisUpdate) {
        if (fingerOn) {
            Serial.println("\nHR: No finger detected");
            resetRates();
        }
        fingerOn = false;
    }
}

// Get current BPM
float heartRateGetBPM()        { return averageBPM; }
bool  heartRateFingerPresent() { return fingerOn; }

// Check if BPM is abnormal
bool heartRateIsAbnormal() {
    if (!fingerOn || averageBPM == 0) {
        return false;
    }
    return (averageBPM > (float)HR_ABNORMAL_HIGH || averageBPM < (float)HR_ABNORMAL_LOW);
}
