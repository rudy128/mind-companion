// =============================================================
// LED Breathing Pattern Implementation (non-blocking)
// =============================================================

#include "led_breathing.h"
#include "../config.h"
/*
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

*/



//--------------------------sin wave direct
#include "led_breathing.h"
#include <math.h>
#include "config.h"

// ─── LEDC configuration ─────────────────
static const int LED_CHANNEL = 0;
static const int LED_FREQ = 5000;
static const int LED_RES = 8;

// ─── Breathing state ────────────────────
static bool breathingActive = false;
static unsigned long startTime = 0;

// breathing cycle (ms)
static const float BREATH_PERIOD = 8000.0;   // 8 seconds full breath

// ────────────────────────────────────────
void ledBreathingInit()
{
    ledcSetup(LED_CHANNEL, LED_FREQ, LED_RES);
    ledcAttachPin(LED_PIN, LED_CHANNEL);

    ledcWrite(LED_CHANNEL, 0);
}

// ────────────────────────────────────────
void ledBreathingStart()
{
    breathingActive = true;
    startTime = millis();
}

// ────────────────────────────────────────
void ledBreathingStop()
{
    breathingActive = false;
    ledcWrite(LED_CHANNEL, 0);
}

// ────────────────────────────────────────
bool ledBreathingIsActive()
{
    return breathingActive;
}

// ────────────────────────────────────────
bool ledBreathingUpdate()
{
    if (!breathingActive)
        return false;

    float elapsed = millis() - startTime;

    // convert to sine phase
    float phase = (elapsed / BREATH_PERIOD) * 2 * PI;

    // sine wave (-1 to 1)
    float wave = sin(phase);

    // normalize to 0–1
    float brightness = (wave + 1.0) / 2.0;

    // gamma correction for smoother LED perception
    brightness = pow(brightness, 2.2);

    int pwm = brightness * 255;

    ledcWrite(LED_CHANNEL, pwm);

    return true;
}