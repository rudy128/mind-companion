// Mirrors the user's Python reference (T / T-1 same → random ±(5..7), then
// even t: opposite sign ±2 from base; odd t: decrement magnitude, same sign).
// While Active: if rounded sensor differs from either of the last two sensor
// samples, abort → return real sensor BPM and wait for a new trigger.

#include "bpm_stuck_shuffle.h"

#include <Arduino.h>
#if defined(ESP_PLATFORM)
#include <esp_random.h>
#endif

namespace {

constexpr int kBpmClampLo = 40;
constexpr int kBpmClampHi = 220;

// After an episode completes, ignore same-value triggers briefly so a flat
// sensor does not immediately restart the animation every second.
constexpr unsigned long kRetriggerCooldownMs = 2500;

enum class Phase { Idle, Active };

Phase     phase           = Phase::Idle;
int       prevSensor      = -1;
int       animBase        = 0;
bool      signPlus        = false;
int       num             = 0;   // 5..7, then decremented on odd Python t >= 3
int       episodeT        = 2;   // Python loop variable t (starts at 2)
unsigned long cooldownUntil = 0;

// Last two **sensor** rounded values (while Active). Used to abort the offset
// sequence if the real BPM moves off a flat triple (curr != prev or curr != prev2).
int       fluctPrev1      = -1;
int       fluctPrev2      = -1;

static int applyOffset(int base, bool plus, int magnitude) {
    long v = plus ? (long)base + magnitude : (long)base - magnitude;
    if (v < kBpmClampLo) {
        v = kBpmClampLo;
    }
    if (v > kBpmClampHi) {
        v = kBpmClampHi;
    }
    return (int)v;
}

static void pickRandomDraw() {
    num      = (int)random(5, 8);   // 5, 6, or 7
    signPlus = (random(0, 2) == 1);
}

} // namespace

int bpmStuckShuffleUpdate(int sensorRounded, bool fingerPresent) {
    static bool rngSeeded = false;
    if (!rngSeeded) {
#if defined(ESP_PLATFORM)
        randomSeed((uint32_t)esp_random());
#else
        randomSeed((uint32_t)millis());
#endif
        rngSeeded = true;
    }

    const unsigned long now = millis();

    if (!fingerPresent || sensorRounded <= 0) {
        phase        = Phase::Idle;
        prevSensor   = -1;
        fluctPrev1   = -1;
        fluctPrev2   = -1;
        return sensorRounded;
    }

    if (phase == Phase::Idle) {
        const bool cooledDown = (now >= cooldownUntil);
        const bool twoSame =
            (prevSensor == sensorRounded) && (sensorRounded > 0) && (prevSensor > 0);

        if (cooledDown && twoSame) {
            phase       = Phase::Active;
            animBase    = sensorRounded;
            episodeT    = 2;
            fluctPrev1  = sensorRounded;
            fluctPrev2  = sensorRounded;
            pickRandomDraw();
        }

        prevSensor = sensorRounded;

        if (phase == Phase::Idle) {
            return sensorRounded;
        }
        // Fall through: first Active tick — compute t == 2 below
    }

    // Active: if real sensor BPM moved vs last two samples, stop offsetting.
    if (sensorRounded != fluctPrev1 || sensorRounded != fluctPrev2) {
        phase         = Phase::Idle;
        prevSensor    = sensorRounded;
        fluctPrev1    = -1;
        fluctPrev2    = -1;
        cooldownUntil = now + kRetriggerCooldownMs;
        return sensorRounded;
    }

    // Active: compute Python step for current episodeT
    int result;
    if (episodeT == 2) {
        result = applyOffset(animBase, signPlus, num);
    } else if ((episodeT % 2) == 0) {
        const bool oppositePlus = !signPlus;
        result = applyOffset(animBase, oppositePlus, 2);
    } else {
        num -= 1;
        if (num < 0) {
            num = 0;
        }
        result = applyOffset(animBase, signPlus, num);
    }

    if (result == animBase) {
        phase         = Phase::Idle;
        prevSensor    = sensorRounded;
        fluctPrev1    = -1;
        fluctPrev2    = -1;
        cooldownUntil = now + kRetriggerCooldownMs;
        return result;
    }

    episodeT += 1;
    // Failsafe: never spin forever if base/clamp prevents reaching animBase
    if (episodeT > 64) {
        phase         = Phase::Idle;
        prevSensor    = sensorRounded;
        fluctPrev1    = -1;
        fluctPrev2    = -1;
        cooldownUntil = now + kRetriggerCooldownMs;
        return sensorRounded;
    }

    fluctPrev2 = fluctPrev1;
    fluctPrev1 = sensorRounded;

    return result;
}
