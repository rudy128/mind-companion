// =============================================================
// LED Breathing Pattern Implementation (non-blocking)
// =============================================================
#include "led_breathing.h"
#include "../config.h"

static bool  active       = false;
static int   totalCycles  = 0;
static int   currentCycle = 0;
static int   brightness   = 0;
static bool  rising       = true;
static unsigned long lastStep = 0;
static const unsigned long STEP_MS = 15; // ms per brightness step

void ledBreathingInit() {
    // ESP32 Arduino Core 2.x PWM API (compatible with Core 2.x and older 3.x)
    ledcSetup(0, LED_PWM_FREQ, LED_PWM_RESOLUTION);
    ledcAttachPin(LED_PIN, 0);
    ledcWrite(0, 255);
    Serial.println("[LED] Breathing LED initialized on GPIO" + String(LED_PIN));
}

void ledBreathingStart(int cycles) {
    totalCycles  = cycles;
    currentCycle = 0;
    brightness   = 0;
    rising       = true;
    active       = true;
    lastStep     = millis();
    Serial.println("[LED] Breathing started (" + String(cycles) + " cycles)");
}

bool ledBreathingUpdate() {
    if (!active) return false;

    unsigned long now = millis();
    if (now - lastStep < STEP_MS) return true; // not time yet
    lastStep = now;

    if (rising) {
        brightness += 5;
        if (brightness >= 255) {
            brightness = 255;
            rising = false;
        }
    } else {
        brightness -= 5;
        if (brightness <= 0) {
            brightness = 0;
            rising = true;
            currentCycle++;
            if (currentCycle >= totalCycles) {
                active = false;
                ledcWrite(0, 0);
                Serial.println("[LED] Breathing complete");
                return false;
            }
        }
    }

    ledcWrite(0, brightness);
    return true;
}

void ledBreathingStop() {
    active = false;
    ledcWrite(0, 255);
}

bool ledBreathingIsActive() {
    return active;
}
