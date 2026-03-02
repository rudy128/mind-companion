// =============================================================
// Sleep Detector Implementation
// 30-sample ring buffer (1 sample/sec = 30-second window).
//
// Calibrated classification:
//   avgActiveAxes >= 2.5 AND avgDelta > 0.8  → Awake
//     (all 3 axes firing consistently = deliberate movement)
//   avgActiveAxes >= 1.0 AND avgDelta > 0.15 → Light Sleep
//     (1-2 axes shifting = restless lying down)
//   otherwise                                 → Deep Sleep
// =============================================================
#include "sleep_detector.h"
#include "../config.h"

#define SLEEP_WINDOW_SAMPLES 30   // 30 samples × 1 s = 30-second window

static float         deltaBuffer[SLEEP_WINDOW_SAMPLES];
static float         axesBuffer[SLEEP_WINDOW_SAMPLES];  // active-axes count per sample
static int           bufferIndex    = 0;
static bool          bufferFull     = false;
static String        quality        = "Awake";

void sleepDetectorInit() {
    memset(deltaBuffer, 0, sizeof(deltaBuffer));
    memset(axesBuffer,  0, sizeof(axesBuffer));
    bufferIndex = 0;
    bufferFull  = false;
    quality     = "Awake";
    Serial.println("[SLEEP] Detector initialized (30-sample ring buffer, 1 s/sample)");
}

void sleepDetectorFeed(float dx, float dy, float dz, int activeAxes) {
    float totalDelta = dx + dy + dz;

    // Store both delta and axis count in ring buffers
    deltaBuffer[bufferIndex] = totalDelta;
    axesBuffer[bufferIndex]  = (float)activeAxes;
    bufferIndex++;

    if (bufferIndex >= SLEEP_WINDOW_SAMPLES) {
        bufferFull  = true;
        bufferIndex = 0;
    }

    // Only evaluate once we have a full 30-sample window
    if (!bufferFull) return;

    float deltaSum = 0, axesSum = 0;
    for (int i = 0; i < SLEEP_WINDOW_SAMPLES; i++) {
        deltaSum += deltaBuffer[i];
        axesSum  += axesBuffer[i];
    }
    float avgDelta      = deltaSum / SLEEP_WINDOW_SAMPLES;
    float avgActiveAxes = axesSum  / SLEEP_WINDOW_SAMPLES;

    // Calibrated thresholds — derived from real serial log observations:
    //
    //   Observed ranges on this hardware (MPU6050 accel, threshold 0.3 g):
    //     Full board movement : avgDelta ≈ 2.5–3.5,  avgAxes ≈ 1.5–2.0
    //     Restless / fidget   : avgDelta ≈ 0.3–1.5,  avgAxes ≈ 0.5–1.2
    //     True still          : avgDelta < 0.10,     avgAxes ≈ 0.00
    //
    //   avgAxes ALONE is not a reliable discriminator because the MPU
    //   gravity vector always contributes to Z even when the board is
    //   stationary; full deliberate movement never reliably hits 3.
    //   → Use avgDelta as the primary signal, avgAxes as a gate only.
    //
    //   Awake       : avgDelta > 1.5  (strong, sustained movement)
    //   Light Sleep : avgDelta > 0.10 (some movement but weak/intermittent)
    //   Deep Sleep  : avgDelta ≤ 0.10 (essentially motionless)
    if (avgDelta > 1.5f) {
        quality = "Awake";
    } else if (avgDelta > 0.10f) {
        quality = "Light Sleep";
    } else {
        quality = "Deep Sleep";
    }

    Serial.printf("[SLEEP] avgDelta=%.4f avgAxes=%.2f → %s\n",
                  avgDelta, avgActiveAxes, quality.c_str());
}

String sleepDetectorGetQuality() {
    return quality;
}

bool sleepDetectorIsSleeping() {
    return (quality == "Deep Sleep" || quality == "Light Sleep");
}
