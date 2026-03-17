// =============================================================
// LED Breathing Pattern — Non-blocking linear fade (PNP inverted)
//
// Pure repeating worker — NO timer logic. Orchestration (main.cpp) calls
// ledBreathingStop() when the session should end.
//
// Fade-in  : linear duty 255→0  (keeps the nice feel the user confirmed)
// Fade-out : gamma-corrected (γ=2) so the LED completes cleanly instead
//            of lingering at near-zero brightness for the last ~50 steps
// Pause    : 400 ms between cycles
// Cycle    : ≈ 2.95 s total
// =============================================================
#include "led_breathing.h"
#include "../config.h"

// ── LEDC config ───────────────────────────────────────────────
static const int CHANNEL = 0;
static const int FREQ    = 5000;
static const int RES     = 8;

// ── Timing — ~3 s per full cycle ─────────────────────────────
//   Fade in : 255 steps × 5 ms = 1.275 s
//   Fade out: 255 steps × 5 ms = 1.275 s  (gamma-corrected write)
//   Pause   : 0.4 s
static const unsigned long STEP_MS  = 5;    // ms per duty step
static const unsigned long PAUSE_MS = 400;  // ms pause at end of each cycle

// ── State machine ─────────────────────────────────────────────
static bool          _active       = false;
static bool          _fadingIn     = true;   // true  = duty 255→0 (brighten)
                                             // false = duty 0→255 (dim)
static bool          _pausing      = false;
static bool          _stopping     = false;  // graceful-stop requested
static int           _duty         = 255;    // PNP: 255 = off (start dark)
static unsigned long _lastStepMs   = 0;
static unsigned long _pauseStartMs = 0;

// ── Public API ────────────────────────────────────────────────

void ledBreathingInit() {
    ledcSetup(CHANNEL, FREQ, RES);
    ledcAttachPin(LED_PIN, CHANNEL);
    ledcWrite(CHANNEL, 255);   // LED off on boot
}

void ledBreathingStart() {
    _active      = true;
    _fadingIn    = true;
    _stopping    = false;
    _duty        = 255;        // begin from LED-off
    _pausing     = false;
    _lastStepMs  = millis();
}

void ledBreathingStop() {
    _active   = false;
    _stopping = false;
    ledcWrite(CHANNEL, 255);   // LED off (PNP: 255 = off)
}

void ledBreathingGracefulStop() {
    if (!_active) return;
    _stopping = true;
    // If sitting in the inter-cycle pause the LED is already dark — just finish.
    if (_pausing) {
        _active   = false;
        _stopping = false;
        ledcWrite(CHANNEL, 255);
    }
    // Otherwise update() will handle the wind-down.
}

bool ledBreathingIsActive() { return _active; }

bool ledBreathingUpdate() {
    if (!_active) return false;

    unsigned long now = millis();

    // ── Pause between cycles ─────────────────────────────────
    if (_pausing) {
        if (_stopping) {
            // Graceful stop: LED is already off during pause — done.
            _active   = false;
            _stopping = false;
            ledcWrite(CHANNEL, 255);
            return false;
        }
        if (now - _pauseStartMs >= PAUSE_MS) {
            _pausing  = false;
            _fadingIn = true;
            _duty     = 255;    // restart from LED-off
        }
        return true;
    }

    // If graceful-stop requested mid fade-in, reverse to fade-out immediately
    if (_stopping && _fadingIn) {
        _fadingIn = false;
    }

    // ── Rate-limit to STEP_MS ────────────────────────────────
    if (now - _lastStepMs < STEP_MS) return true;
    _lastStepMs = now;

    if (_fadingIn) {
        // ── Fade in: linear (user confirmed this feels right) ──
        // duty 255 → 0: PNP decreasing duty = brighter
        _duty--;
        if (_duty <= 0) {
            _duty     = 0;
            _fadingIn = false;
        }
        ledcWrite(CHANNEL, _duty);

    } else {
        // ── Fade out: gamma-corrected (γ=2) ───────────────────
        _duty++;
        if (_duty >= 255) {
            _duty = 255;
            if (_stopping) {
                // Graceful stop complete — LED fully off, we're done.
                _active   = false;
                _stopping = false;
                ledcWrite(CHANNEL, 255);
                return false;
            }
            _pausing       = true;
            _pauseStartMs  = now;
        }
        float    on  = (255.0f - _duty) / 255.0f;
        on           = on * on;   // γ = 2
        uint8_t  pwm = (uint8_t)((1.0f - on) * 255.0f + 0.5f);
        ledcWrite(CHANNEL, pwm);
    }

    return true;
}