// =============================================================
// LED Breathing Pattern Implementation (non-blocking)
// =============================================================

#include "led_breathing.h"
#include "../config.h"

static bool active = false;
static bool fadingIn = true;

static int brightness = 255;   // OFF initially (PNP)

static unsigned long lastStep = 0;

// 4 sec inhale + 4 sec exhale
// ~155 steps * 15ms ≈ 2.3 sec each direction
static const unsigned long STEP_MS = 15;
static const int STEP_SIZE = 1;

// 🔹 New brightness limits
static const int MIN_BRIGHTNESS = 200;   // dimmest point
static const int MAX_BRIGHTNESS = 255;   // fully off (PNP)

void ledBreathingInit()
{
    ledcSetup(0, LED_PWM_FREQ, LED_PWM_RESOLUTION);
    ledcAttachPin(LED_PIN, 0);
    ledcWrite(0, MAX_BRIGHTNESS);  // OFF
}

void ledBreathingStart(int cycles)
{
    brightness = MAX_BRIGHTNESS;  // start OFF
    fadingIn = true;
    active = true;
    lastStep = millis();
}

bool ledBreathingUpdate()
{
    if (!active) return false;

    unsigned long now = millis();
    if (now - lastStep < STEP_MS) return true;
    lastStep = now;

    if (fadingIn)
    {
        brightness -= STEP_SIZE;   // Inhale (brighter for PNP)

        if (brightness <= MIN_BRIGHTNESS)
        {
            brightness = MIN_BRIGHTNESS;
            fadingIn = false;
        }
    }
    else
    {
        brightness += STEP_SIZE;   // Exhale (dimmer for PNP)

        if (brightness >= MAX_BRIGHTNESS)
        {
            brightness = MAX_BRIGHTNESS;
            fadingIn = true;
        }
    }

    ledcWrite(0, brightness);
    return true;
}

void ledBreathingStop()
{
    active = false;
    ledcWrite(0, MAX_BRIGHTNESS);  // OFF
}

bool ledBreathingIsActive()
{
    return active;
}


