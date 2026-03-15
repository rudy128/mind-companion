// =============================================================
// LED Breathing Pattern — Non-blocking linear fade (PNP inverted)
//
// Pure repeating worker — matches the working standalone sketch:
//   Fade in : duty 255 → 0   (LED brightens, 10 ms/step ≈ 2.55 s)
//   Fade out: duty 0   → 255 (LED dims,      10 ms/step ≈ 2.55 s)
//   Pause   : 1 s between cycles
//
// NO timer logic here — orchestration (main.cpp) owns start/stop timing.
// The LED will keep cycling until ledBreathingStop() is called externally.
// =============================================================
#include "led_breathing.h"
#include "../config.h"

// ── LEDC config ───────────────────────────────────────────────
static const int CHANNEL = 0;
static const int FREQ    = 5000;
static const int RES     = 8;

// ── Fade timing — ~3 s per full cycle ───────────────────────
//   Fade in : 255 steps × 5 ms = 1.275 s
//   Fade out: 255 steps × 5 ms = 1.275 s
//   Pause   : 0.4 s
//   Total   : ≈ 2.95 s
static const unsigned long STEP_MS  = 5;     // ms per duty step
static const unsigned long PAUSE_MS = 400;   // ms pause at end of each cycle

// ── State machine ─────────────────────────────────────────────
static bool          _active       = false;
static bool          _fadingIn     = true;   // true  = duty 255→0 (brighten)
                                             // false = duty 0→255 (dim)
static bool          _pausing      = false;
static int           _duty         = 255;    // PNP: 255 = LED off (start dark)
static unsigned long _lastStepMs   = 0;
static unsigned long _pauseStartMs = 0;

// ── Public API ────────────────────────────────────────────────

void ledBreathingInit() {
    ledcSetup(CHANNEL, FREQ, RES);
    ledcAttachPin(LED_PIN, CHANNEL);
    ledcWrite(CHANNEL, 255);   // ensure LED is off on boot
}

void ledBreathingStart() {
    _active       = true;
    _fadingIn     = true;
    _duty         = 255;       // begin from LED-off
    _pausing      = false;
    _lastStepMs   = millis();
}

void ledBreathingStop() {
    _active = false;
    ledcWrite(CHANNEL, 255);   // LED off (PNP: 255 = off)
}

bool ledBreathingIsActive() { return _active; }

bool ledBreathingUpdate() {
    if (!_active) return false;

    unsigned long now = millis();

    // ── Pause between cycles ─────────────────────────────────
    if (_pausing) {
        if (now - _pauseStartMs >= PAUSE_MS) {
            _pausing  = false;
            _fadingIn = true;
            _duty     = 255;     // restart from LED-off
        }
        return true;
    }

    // ── Rate-limit to STEP_MS ────────────────────────────────
    if (now - _lastStepMs < STEP_MS) return true;
    _lastStepMs = now;

    if (_fadingIn) {
        // Fade in: 255 → 0  (PNP: decreasing duty = brighter)
        _duty--;
        if (_duty <= 0) {
            _duty     = 0;
            _fadingIn = false;
        }
    } else {
        // Fade out: 0 → 255  (PNP: increasing duty = dimmer)
        _duty++;
        if (_duty >= 255) {
            _duty          = 255;
            _pausing       = true;
            _pauseStartMs  = now;
        }
    }

    ledcWrite(CHANNEL, _duty);
    return true;
}