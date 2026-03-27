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

static float         deltaBuffer[SLEEP_WINDOW_SAMPLES];  //total movement
static float         axesBuffer[SLEEP_WINDOW_SAMPLES];  // axes with significant movement
static int           bufferIndex    = 0;
static bool          bufferFull     = false;
static String        quality        = "Awake";          //Current sleep quality

//Initalize sleep detector
void sleepDetectorInit() {
    memset(deltaBuffer, 0, sizeof(deltaBuffer));
    memset(axesBuffer,  0, sizeof(axesBuffer));
    bufferIndex = 0;
    bufferFull  = false;
    quality     = "Awake";
    Serial.println("[SLEEP] Detector initialized (30-sample ring buffer, 1 s/sample)");
}

//Feed new movement data (called every second)
void sleepDetectorFeed(float dx, float dy, float dz, int activeAxes) {
    float totalDelta = dx + dy + dz;            //total movement

    // Store both delta and axis count in buffer
    deltaBuffer[bufferIndex] = totalDelta;
    axesBuffer[bufferIndex]  = (float)activeAxes;
    bufferIndex++;

    // Reset index when buffer is full
    if (bufferIndex >= SLEEP_WINDOW_SAMPLES) {
        bufferFull  = true;
        bufferIndex = 0;
    }

    if (!bufferFull) return;

    //Calculate average 
    float deltaSum = 0, axesSum = 0;
    for (int i = 0; i < SLEEP_WINDOW_SAMPLES; i++) {
        deltaSum += deltaBuffer[i];
        axesSum  += axesBuffer[i];
    }
    float avgDelta      = deltaSum / SLEEP_WINDOW_SAMPLES;
    float avgActiveAxes = axesSum  / SLEEP_WINDOW_SAMPLES;


    // Decide sleep level based on movement thresholds
    if (avgDelta > 1.5f) {              
        quality = "Awake";            //lots of movement across multiple axes 
    } else if (avgDelta > 0.10f) {
        quality = "Light Sleep";      //small movement or movement on few axes 
    } else {
        quality = "Deep Sleep";      //almost no movement 
    }

    Serial.printf("[SLEEP] avgDelta=%.4f avgAxes=%.2f → %s\n",
                  avgDelta, avgActiveAxes, quality.c_str());
}

// Get current sleep quality
String sleepDetectorGetQuality() {
    return quality;
}
