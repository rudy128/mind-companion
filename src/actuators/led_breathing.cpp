// =============================================================
// LED Breathing Pattern (LEDC PWM Smooth Fade, Non-blocking)
// =============================================================
#include "led_breathing.h"
#include "../config.h"
#include "../network/logger.h"

// PNP transistor logic:
//   duty = 255  →  transistor OFF  →  LED OFF
//   duty = 0    →  transistor ON   →  LED fully ON
// Fade IN  = duty 255 → 0  (LED brightens)
// Fade OUT = duty 0   → 255 (LED dims)

static bool          active       = false;
static bool          fadingIn     = true;
static int           duty         = 255;   // use int to safely detect over/underflow
static unsigned long lastStep     = 0;
static int           totalCycles  = 0;
static int           currentCycle = 0;

// 255 steps × 12 ms ≈ 3 s per half → ~6 s per full breath
static const int           STEP_SIZE = 1;
static const unsigned long STEP_MS   = 12;

void ledBreathingInit() {
    ledcSetup(0, LED_PWM_FREQ, LED_PWM_RESOLUTION);
    ledcAttachPin(LED_PIN, 0);
    ledcWrite(0, 255);  // start OFF
    LOG_INFO("LED", "Init — PWM ch0 pin %d @ %d Hz", LED_PIN, LED_PWM_FREQ);
}

void ledBreathingStart(int cycles) {
    totalCycles  = cycles;
    currentCycle = 0;
    duty         = 255;
    fadingIn     = true;
    active       = true;
    lastStep     = millis();
    ledcWrite(0, 255);
    LOG_INFO("LED", "START — %d cycles (~%d s)", cycles, cycles * 6);
}

bool ledBreathingUpdate() {
    if (!active) return false;

    unsigned long now = millis();
    if (now - lastStep < STEP_MS) return true;
    lastStep = now;

    // Advance duty one step
    duty += fadingIn ? -STEP_SIZE : STEP_SIZE;

    // Clamp and detect direction flip
    if (duty <= 0) {
        duty = 0;
        fadingIn = false;
        LOG_DEBUG("LED", "Cycle %d/%d — full brightness, fading out",
                  currentCycle + 1, totalCycles);
    } else if (duty >= 255) {
        duty = 255;
        fadingIn = true;
        currentCycle++;
        LOG_DEBUG("LED", "Cycle %d/%d — fully off, cycle done",
                  currentCycle, totalCycles);
        if (totalCycles > 0 && currentCycle >= totalCycles) {
            active = false;
            ledcWrite(0, 255);
            LOG_INFO("LED", "DONE — %d cycles complete", totalCycles);
            return false;
        }
    }

    ledcWrite(0, (uint32_t)duty);
    return true;
}

void ledBreathingStop() {
    active = false;
    ledcWrite(0, 255);
    LOG_INFO("LED", "STOPPED at cycle %d/%d (duty=%d)", currentCycle, totalCycles, duty);
}

bool ledBreathingIsActive() {
    return active;
}


