// =============================================================
// Speaker — MAX98357A I2S Implementation + Alarm
// =============================================================
#include "speaker.h"
#include "../config.h"
#include "driver/i2s.h"
#include <math.h>

static bool          alarmActive = false;
static unsigned long alarmToggle = 0;
static bool          alarmHigh   = false;   // alternates between two tones

void speakerInit() {
    Serial.println("[SPK] Initializing I2S #1 (TX)...");

    i2s_config_t i2s_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = SPK_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = 0,
        .dma_buf_count        = 8,
        .dma_buf_len          = 256,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
        .fixed_mclk           = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num   = SPK_I2S_BCLK,
        .ws_io_num    = SPK_I2S_LRC,
        .data_out_num = SPK_I2S_DIN,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(SPK_I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(SPK_I2S_PORT, &pin_config);
    Serial.println("[SPK] I2S #1 OK");
}

void speakerPlayTone(float freqHz, unsigned long durationMs) {
    const int amplitude = 15000;
    unsigned long samples = (SPK_SAMPLE_RATE * durationMs) / 1000;
    int16_t buf[256];
    size_t written;
    unsigned long idx = 0;

    while (idx < samples) {
        int count = min((unsigned long)256, samples - idx);
        for (int i = 0; i < count; i++) {
            buf[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * freqHz * (idx + i) / SPK_SAMPLE_RATE));
        }
        i2s_write(SPK_I2S_PORT, buf, count * sizeof(int16_t), &written, portMAX_DELAY);
        idx += count;
    }

    // Silence after tone
    memset(buf, 0, sizeof(buf));
    i2s_write(SPK_I2S_PORT, buf, sizeof(buf), &written, portMAX_DELAY);
}

void speakerBeepOK() {
    speakerPlayTone(880, 100);
    delay(50);
    speakerPlayTone(1100, 100);
}

void speakerBeepError() {
    speakerPlayTone(440, 500);
}

// ============ Non-blocking alarm (replaces buzzer) ============

void speakerAlarmStart() {
    alarmActive = true;
    alarmToggle = millis();
    alarmHigh   = false;
    Serial.println("[SPK] Alarm started!");
}

void speakerAlarmStop() {
    alarmActive = false;
    // Push a short silence to flush the I2S buffer
    int16_t silence[256] = {0};
    size_t written;
    i2s_write(SPK_I2S_PORT, silence, sizeof(silence), &written, portMAX_DELAY);
    Serial.println("[SPK] Alarm stopped");
}

bool speakerAlarmUpdate() {
    if (!alarmActive) return false;

    // Alternate between two alarm tones every 400 ms
    if (millis() - alarmToggle >= 400) {
        alarmHigh = !alarmHigh;
        float freq = alarmHigh ? 1200.0f : 800.0f;
        // Play a short burst (non-blocking-ish: 100 ms tone is fast enough)
        speakerPlayTone(freq, 100);
        alarmToggle = millis();
    }
    return true;
}

bool speakerAlarmIsActive() {
    return alarmActive;
}
