// =============================================================
// LED Breathing Pattern — Time-based smooth fade (PNP inverted)
//
// Uses elapsed-time interpolation instead of discrete steps.
// Full cycle: 500 ms fade-in + 500 ms fade-out + 150 ms rest = ~1.15 s
// Zero dwell at peak brightness — reversal is instant.
// =============================================================
#include "led_breathing.h"
#include "../config.h"

// ── LEDC config ───────────────────────────────────────────────
static const int CHANNEL = 0;
static const int FREQ    = 5000;
static const int RES     = 8;

// ── Timing ────────────────────────────────────────────────────
static const unsigned long FADE_IN_MS  = 500;   // dark → full bright
static const unsigned long FADE_OUT_MS = 500;   // full bright → dark
static const unsigned long PAUSE_MS    = 150;   // rest between cycles (LED off)

// ── State: 0 = fade-in, 1 = fade-out, 2 = pause ─────────────
static bool          _active       = false;
static bool          _stopping     = false;
static uint8_t       _phase        = 0;
static unsigned long _phaseStartMs = 0;

// ── Public API ────────────────────────────────────────────────

void ledBreathingInit() {
    ledcSetup(CHANNEL, FREQ, RES);
    ledcAttachPin(LED_PIN, CHANNEL);
    ledcWrite(CHANNEL, 255);   // PNP: 255 = LED off
}

void ledBreathingStart() {
    _active       = true;
    _stopping     = false;
    _phase        = 0;         // start with fade-in
    _phaseStartMs = millis();
}

void ledBreathingStop() {
    _active   = false;
    _stopping = false;
    ledcWrite(CHANNEL, 255);
}

void ledBreathingGracefulStop() {
    if (!_active) return;
    _stopping = true;
    if (_phase == 2) {
        _active   = false;
        _stopping = false;
        ledcWrite(CHANNEL, 255);
    }
}

bool ledBreathingIsActive() { return _active; }

bool ledBreathingUpdate() {
    if (!_active) return false;

    unsigned long now     = millis();
    unsigned long elapsed = now - _phaseStartMs;

    // ── Graceful stop mid fade-in: jump into fade-out at matching brightness
    if (_stopping && _phase == 0) {
        float t_in   = constrain((float)elapsed / FADE_IN_MS, 0.0f, 1.0f);
        // Current PNP duty: 255 at t=0 (off), 0 at t=1 (full on)
        uint8_t curDuty = (uint8_t)(255.0f * (1.0f - t_in) + 0.5f);
        // Pre-wind fade-out timer so it starts from the same brightness
        float t_out  = (float)curDuty / 255.0f;   // 0 = full on, 1 = off
        _phase        = 1;
        _phaseStartMs = now - (unsigned long)(t_out * FADE_OUT_MS);
        elapsed       = now - _phaseStartMs;
    }

    if (_phase == 0) {
        // ── Fade in: PNP duty 255 → 0 (off → full bright) ────────
        float t = constrain((float)elapsed / FADE_IN_MS, 0.0f, 1.0f);
        uint8_t duty = (uint8_t)(255.0f * (1.0f - t) + 0.5f);
        ledcWrite(CHANNEL, duty);

        if (t >= 1.0f) {
            _phase        = 1;
            _phaseStartMs = now;
        }
    }
    else if (_phase == 1) {
        // ── Fade out: PNP duty 0 → 255 (full bright → off) ──────
        float t = constrain((float)elapsed / FADE_OUT_MS, 0.0f, 1.0f);
        uint8_t duty = (uint8_t)(255.0f * t + 0.5f);
        ledcWrite(CHANNEL, duty);

        if (t >= 1.0f) {
            ledcWrite(CHANNEL, 255);   // ensure fully off
            if (_stopping) {
                _active   = false;
                _stopping = false;
                return false;
            }
            _phase        = 2;
            _phaseStartMs = now;
        }
    }
    else {
        // ── Pause: LED is off, short rest between breaths ────────
        if (_stopping) {
            _active   = false;
            _stopping = false;
            ledcWrite(CHANNEL, 255);
            return false;
        }
        if (elapsed >= PAUSE_MS) {
            _phase        = 0;
            _phaseStartMs = now;
        }
    }

    return true;
}
