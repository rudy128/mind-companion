// =============================================================
// LED Breathing Pattern — Non-blocking linear fade (PNP inverted)
//
// Replicates the working standalone sketch:
//   Fade in : duty 255 → 0   (LED brightens, 10 ms/step ≈ 2.55 s)
//   Fade out: duty 0   → 255 (LED dims,      10 ms/step ≈ 2.55 s)
//   Pause   : 1 second between cycles
//
// Two trigger modes come from the outside:
//   ledBreathingStart()            → indefinite  (HR triggered / dashboard cmd)
//   ledBreathingStartTimed(ms)     → auto-stops  (voice command, ~35 s)
// =============================================================
#include "led_breathing.h"
#include "../config.h"

// ── LEDC config (must match config.h LED_PIN) ────────────────
static const int CHANNEL = 0;
static const int FREQ    = 5000;
static const int RES     = 8;

// ── Fade timing (matches working standalone sketch) ──────────
static const unsigned long STEP_MS  = 10;    // 10 ms per duty step → 2.55 s fade
static const unsigned long PAUSE_MS = 1000;  // 1 s pause at end of each cycle

// ── State machine ────────────────────────────────────────────
static bool          _active        = false;
static bool          _fadingIn      = true;  // true  = 255→0  (brighten PNP)
                                             // false = 0→255  (dim PNP)
static bool          _pausing       = false;
static int           _duty          = 255;   // start LED off (PNP: 255 = off)
static unsigned long _lastStepMs    = 0;
static unsigned long _pauseStartMs  = 0;

// ── Timed mode ───────────────────────────────────────────────
static bool          _timedMode     = false;
static unsigned long _endTimeMs     = 0;     // millis() at which timed mode expires

// ── Public API ───────────────────────────────────────────────

void ledBreathingInit() {
    ledcSetup(CHANNEL, FREQ, RES);
    ledcAttachPin(LED_PIN, CHANNEL);
    ledcWrite(CHANNEL, 255);  // LED off (PNP inverted)
}

static void _startInternal() {
    _active       = true;
    _fadingIn     = true;
    _duty         = 255;    // begin from LED-off state
    _pausing      = false;
    _lastStepMs   = millis();
}

void ledBreathingStart() {
    _timedMode    = false;
    _endTimeMs    = 0;
    _startInternal();
}

void ledBreathingStartTimed(unsigned long durationMs) {
    _timedMode    = true;
    _endTimeMs    = millis() + durationMs;
    _startInternal();
}

void ledBreathingStop() {
    _active     = false;
    _timedMode  = false;
    _endTimeMs  = 0;
    ledcWrite(CHANNEL, 255);   // LED off
}

bool ledBreathingIsActive()   { return _active; }
bool ledBreathingIsTimedMode() { return _active && _timedMode; }

bool ledBreathingUpdate() {
    if (!_active) return false;

    unsigned long now = millis();

    // ── Timed-mode expiry check ──────────────────────────────
    if (_timedMode && _endTimeMs > 0 && now >= _endTimeMs) {
        ledBreathingStop();
        return false;
    }

    // ── Pause between cycles ─────────────────────────────────
    if (_pausing) {
        if (now - _pauseStartMs >= PAUSE_MS) {
            _pausing  = false;
            _fadingIn = true;
            _duty     = 255;          // restart from LED-off
        }
        return true;
    }

    // ── Rate-limit steps to STEP_MS ─────────────────────────
    if (now - _lastStepMs < STEP_MS) return true;
    _lastStepMs = now;

    if (_fadingIn) {
        // Fade in: duty 255 → 0  (PNP: 0 = full brightness)
        _duty--;
        if (_duty <= 0) {
            _duty     = 0;
            _fadingIn = false;  // switch to fade-out
        }
    } else {
        // Fade out: duty 0 → 255  (PNP: 255 = off)
        _duty++;
        if (_duty >= 255) {
            _duty          = 255;
            _pausing       = true;
            _pauseStartMs  = now;   // start 1-second pause
        }
    }

    ledcWrite(CHANNEL, _duty);
    return true;
}