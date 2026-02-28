// =============================================================
// Sleep Detector Implementation
// =============================================================
#include "sleep_detector.h"
#include "../config.h"

static float         movementSum   = 0;
static unsigned long windowStart   = 0;
static String        quality       = "Awake";

void sleepDetectorInit() {
    movementSum = 0;
    windowStart = millis();
    quality     = "Awake";
    Serial.println("[SLEEP] Detector initialized (window=" + String(INTERVAL_SLEEP_WINDOW / 1000) + "s)");
}

void sleepDetectorFeed(float movementDelta) {
    movementSum += movementDelta;

    if (millis() - windowStart >= INTERVAL_SLEEP_WINDOW) {
        // Evaluate window
        if (movementSum < SLEEP_MOVEMENT_DEEP)       quality = "Deep Sleep";
        else if (movementSum < SLEEP_MOVEMENT_LIGHT)  quality = "Light Sleep";
        else                                           quality = "Restless";

        Serial.printf("[SLEEP] Window sum=%.3f → %s\n", movementSum, quality.c_str());

        // Reset for next window
        movementSum = 0;
        windowStart = millis();
    }
}

String sleepDetectorGetQuality() {
    return quality;
}

bool sleepDetectorIsSleeping() {
    return (quality == "Deep Sleep" || quality == "Light Sleep");
}
