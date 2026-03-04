// =============================================================
// Speaker — MAX98357A I2S Implementation + Alarm
// =============================================================
#include "speaker.h"
#include "../config.h"
#include "driver/i2s.h"
#include <math.h>
#include "buzzer_pcm.h"
#include "../network/logger.h"
#include <esp_task_wdt.h>
#include <driver/gpio.h>    // gpio_set_drive_capability()
#include <pgmspace.h>

static bool          alarmActive = false;
static unsigned long alarmToggle = 0;
static bool          alarmHigh   = false;   // alternates between two tones

// Pre-computed one-cycle sine buffers for non-blocking alarm
// 800 Hz @ 22050 Hz sample rate → ~27.5 samples per cycle
// 1200 Hz @ 22050 Hz sample rate → ~18.4 samples per cycle
#define ALARM_BUF_800_LEN   28
#define ALARM_BUF_1200_LEN  18
static int16_t alarmBuf800[ALARM_BUF_800_LEN];
static int16_t alarmBuf1200[ALARM_BUF_1200_LEN];
static unsigned long alarmSamplesWritten = 0;

// Set true while openaiSpeak() is streaming to I2S.
// Alarm and buzzer update functions check this and skip their writes
// to avoid corrupting the TTS stream (both run on different cores).
volatile bool speakerTTSPlaying = false;

// ── I2S mutex — protects the I2S port from concurrent access ──
// Core 1 (alarm/buzzer in loop) and Core 0 (TTS playback) both
// write to SPK_I2S_PORT. This mutex prevents them from colliding.
SemaphoreHandle_t speakerI2SMutex = nullptr;

static void _precomputeAlarmBuffers() {
    const int amplitude = 15000;
    for (int i = 0; i < ALARM_BUF_800_LEN; i++) {
        alarmBuf800[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * 800.0f * i / SPK_SAMPLE_RATE));
    }
    for (int i = 0; i < ALARM_BUF_1200_LEN; i++) {
        alarmBuf1200[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * 1200.0f * i / SPK_SAMPLE_RATE));
    }
}

void speakerInit() {
    LOG_INFO("SPK", "speakerInit() — I2S port %d  BCLK=%d LRC=%d DIN=%d rate=%d",
             (int)SPK_I2S_PORT, SPK_I2S_BCLK, SPK_I2S_LRC, SPK_I2S_DIN, SPK_SAMPLE_RATE);

    // Create the I2S mutex once (survives re-init after TTS playback)
    if (!speakerI2SMutex) {
        speakerI2SMutex = xSemaphoreCreateMutex();
    }

    // Pre-compute sine buffers for non-blocking alarm
    _precomputeAlarmBuffers();

    // Drive SD (shutdown) pin HIGH to enable the MAX98357A amplifier.
    // If SPK_SD_PIN is -1 the amp SD pin is assumed tied directly to 3.3V.
    if (SPK_SD_PIN >= 0) {
        pinMode(SPK_SD_PIN, OUTPUT);
        digitalWrite(SPK_SD_PIN, HIGH);
        LOG_INFO("SPK", "SD pin GPIO%d → HIGH (amp enabled)", SPK_SD_PIN);
    } else {
        LOG_INFO("SPK", "SD pin not configured (assumed tied HIGH externally)");
    }

    // GPIO 0 is the I2S LRC (Word Select) pin on this board.
    // It has a 10kΩ boot pull-down that weakens the WS signal under load.
    // Boosting to GPIO_DRIVE_CAP_3 (strongest) ensures clean transitions.
    gpio_set_drive_capability((gpio_num_t)SPK_I2S_LRC, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability((gpio_num_t)SPK_I2S_BCLK, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability((gpio_num_t)SPK_I2S_DIN, GPIO_DRIVE_CAP_3);
    LOG_INFO("SPK", "I2S pins drive strength → CAP_3 (40mA)");

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

    esp_err_t err = i2s_driver_install(SPK_I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        LOG_ERROR("SPK", "i2s_driver_install FAILED — esp_err=0x%x (%s)",
                  err, esp_err_to_name(err));
    } else {
        LOG_INFO("SPK", "i2s_driver_install OK");
    }

    err = i2s_set_pin(SPK_I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        LOG_ERROR("SPK", "i2s_set_pin FAILED — esp_err=0x%x (%s)",
                  err, esp_err_to_name(err));
    } else {
        LOG_INFO("SPK", "i2s_set_pin OK — I2S ready");
    }
}

void speakerPlayTone(float freqHz, unsigned long durationMs) {
    const int amplitude = 15000;
    const TickType_t kTimeout = 50 / portTICK_PERIOD_MS;

    unsigned long totalSamples = (SPK_SAMPLE_RATE * durationMs) / 1000;
    LOG_DEBUG("SPK", "playTone %.0f Hz %lu ms — %lu samples amplitude=%d timeout=50ms",
              freqHz, durationMs, totalSamples, amplitude);

    int16_t buf[256];
    size_t written;
    unsigned long idx = 0;
    unsigned long totalWritten = 0;
    int timeouts = 0;

    while (idx < totalSamples) {
        int count = (int)min((unsigned long)256, totalSamples - idx);
        for (int i = 0; i < count; i++) {
            buf[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * freqHz * (idx + i) / SPK_SAMPLE_RATE));
        }
        esp_err_t err = i2s_write(SPK_I2S_PORT, buf, count * sizeof(int16_t), &written, kTimeout);
        if (err != ESP_OK) {
            LOG_ERROR("SPK", "i2s_write ERROR 0x%x at sample %lu", err, idx);
        } else if (written < count * sizeof(int16_t)) {
            timeouts++;
            LOG_WARN("SPK", "i2s_write partial: wrote %u of %u bytes (timeout #%d) at sample %lu",
                     written, count * sizeof(int16_t), timeouts, idx);
        }
        totalWritten += written;
        idx += count;
    }

    // Flush silence so the amplifier doesn't click
    memset(buf, 0, sizeof(buf));
    i2s_write(SPK_I2S_PORT, buf, sizeof(buf), &written, kTimeout);

    LOG_DEBUG("SPK", "playTone done — wrote %lu/%lu bytes  timeouts=%d",
              totalWritten, totalSamples * sizeof(int16_t), timeouts);
}

// Stream a raw PCM buffer (16-bit mono, SPK_SAMPLE_RATE) to I2S.
// Used for OpenAI TTS response playback — no SPIFFS needed.
void speakerPlayRaw(const uint8_t* pcmData, size_t pcmBytes) {
    if (!pcmData || pcmBytes == 0) {
        LOG_WARN("SPK", "speakerPlayRaw() called with empty buffer");
        return;
    }
    LOG_INFO("SPK", "speakerPlayRaw() — %u bytes (~%.1f s)",
             pcmBytes, (float)pcmBytes / (SPK_SAMPLE_RATE * 2.0f));

    const size_t chunkBytes = 512;
    size_t offset = 0;
    size_t written = 0;
    int errors = 0;

    while (offset < pcmBytes) {
        size_t toWrite = min(chunkBytes, pcmBytes - offset);
        esp_err_t err = i2s_write(SPK_I2S_PORT, pcmData + offset, toWrite,
                                   &written, portMAX_DELAY);
        if (err != ESP_OK) {
            LOG_ERROR("SPK", "speakerPlayRaw i2s_write ERROR 0x%x at offset %u", err, offset);
            if (++errors > 5) { LOG_ERROR("SPK", "Too many errors, aborting playback"); break; }
        }
        offset += toWrite;
        esp_task_wdt_reset();
    }

    // Flush silence to prevent amp click
    int16_t silence[64] = {0};
    i2s_write(SPK_I2S_PORT, silence, sizeof(silence), &written, portMAX_DELAY);
    LOG_INFO("SPK", "speakerPlayRaw() done — %u/%u bytes  errors=%d", offset, pcmBytes, errors);
}

void speakerBeepOK() {
    speakerPlayTone(880, 100);
    delay(50);
    speakerPlayTone(1100, 100);
    LOG_INFO("SPK", "speakerBeepOK() done");
}

void speakerBeepError() {
    LOG_INFO("SPK", "speakerBeepError() — 440Hz 500ms");
    speakerPlayTone(440, 500);
    LOG_INFO("SPK", "speakerBeepError() done");
}

// ── Speaker hardware test ─────────────────────────────────
void speakerTest() {
    const int      amplitude = 28000;
    const float    freqs[3]  = { 440.0f, 880.0f, 1320.0f };
    const uint32_t durMs     = 400;
    int16_t buf[256];
    size_t  written;

    LOG_INFO("SPK", "=== SPEAKER TEST START === amplitude=%d portMAX_DELAY", amplitude);
    for (int t = 0; t < 3; t++) {
        unsigned long totalSamples = (SPK_SAMPLE_RATE * durMs) / 1000;
        unsigned long idx = 0;
        unsigned long totalWritten = 0;
        LOG_INFO("SPK", "Test tone %d/3 — %.0f Hz  %lu samples", t + 1, freqs[t], totalSamples);
        while (idx < totalSamples) {
            int count = (int)min((unsigned long)256, totalSamples - idx);
            for (int i = 0; i < count; i++) {
                buf[i] = (int16_t)(amplitude *
                          sinf(2.0f * M_PI * freqs[t] * (idx + i) / SPK_SAMPLE_RATE));
            }
            esp_err_t err = i2s_write(SPK_I2S_PORT, buf, count * sizeof(int16_t),
                                       &written, portMAX_DELAY);
            if (err != ESP_OK) {
                LOG_ERROR("SPK", "Test tone %d i2s_write ERROR 0x%x at sample %lu",
                          t + 1, err, idx);
            }
            totalWritten += written;
            idx += count;
        }
        LOG_INFO("SPK", "Test tone %d done — wrote %lu bytes", t + 1, totalWritten);
        delay(100);
    }
    memset(buf, 0, sizeof(buf));
    i2s_write(SPK_I2S_PORT, buf, sizeof(buf), &written, portMAX_DELAY);
    LOG_INFO("SPK", "=== SPEAKER TEST END ===");
}

// ============ Non-blocking alarm (replaces buzzer) ============

void speakerAlarmStart() {
    LOG_WARN("SPK", "speakerAlarmStart() called — alarmActive was %s",
             alarmActive ? "true (already running!)" : "false");
    alarmActive = true;
    alarmToggle = millis();
    alarmHigh   = false;
}

void speakerAlarmStop() {
    LOG_INFO("SPK", "speakerAlarmStop() called — was %s", alarmActive ? "active" : "already stopped");
    alarmActive = false;
    alarmSamplesWritten = 0;

    if (speakerI2SMutex) xSemaphoreTake(speakerI2SMutex, portMAX_DELAY);
    int16_t silence[256] = {0};
    size_t written;
    esp_err_t err = i2s_write(SPK_I2S_PORT, silence, sizeof(silence),
                               &written, 50 / portTICK_PERIOD_MS);
    if (speakerI2SMutex) xSemaphoreGive(speakerI2SMutex);
    LOG_INFO("SPK", "Alarm flush silence — wrote %u bytes err=0x%x", written, err);
}

bool speakerAlarmUpdate() {
    if (!alarmActive) return false;
    if (speakerTTSPlaying) return true;   // TTS owns I2S — skip tick, stay "active"

    // Non-blocking: write ONE chunk of pre-computed sine wave per call.
    // Each chunk takes <1 ms vs the old speakerPlayTone() which blocked ~100 ms.
    // Toggle frequency every ~2200 samples (~100ms at 22050 Hz).

    if (millis() - alarmToggle >= 400) {
        alarmHigh = !alarmHigh;
        alarmSamplesWritten = 0;
        alarmToggle = millis();
        LOG_DEBUG("SPK", "Alarm tick — %.0f Hz", alarmHigh ? 1200.0f : 800.0f);
    }

    // Only write ~2200 samples per tone burst (≈100ms of audio)
    const unsigned long samplesPerBurst = (SPK_SAMPLE_RATE * 100) / 1000;
    if (alarmSamplesWritten >= samplesPerBurst) return true;  // burst done, wait for toggle

    // Pick the right pre-computed buffer
    int16_t* buf   = alarmHigh ? alarmBuf1200 : alarmBuf800;
    int       len  = alarmHigh ? ALARM_BUF_1200_LEN : ALARM_BUF_800_LEN;

    // Try to acquire mutex — if TTS has it, skip this iteration (non-blocking)
    if (speakerI2SMutex && xSemaphoreTake(speakerI2SMutex, 0) != pdTRUE) {
        return true;
    }

    size_t written = 0;
    // Short timeout (10ms) so loop() is never blocked long
    i2s_write(SPK_I2S_PORT, buf, len * sizeof(int16_t), &written, 10 / portTICK_PERIOD_MS);
    alarmSamplesWritten += len;

    if (speakerI2SMutex) xSemaphoreGive(speakerI2SMutex);
    return true;
}

bool speakerAlarmIsActive() {
    return alarmActive;
}

// ============ Non-blocking nudge buzzer (PCM file) ============
static bool          buzzerActive = false;
static unsigned int  buzzerPos    = 0;

void speakerBuzzerStart() {
    if (buzzerActive) {
        LOG_WARN("SPK", "speakerBuzzerStart() — already active, ignoring");
        return;
    }
    LOG_INFO("SPK", "speakerBuzzerStart() — PCM len=%u samples", BUZZER_PCM_LEN);
    buzzerActive = true;
    buzzerPos    = 0;
}

void speakerBuzzerStop() {
    if (!buzzerActive) return;
    LOG_INFO("SPK", "speakerBuzzerStop() at pos=%u/%u", buzzerPos, BUZZER_PCM_LEN);
    buzzerActive = false;
    buzzerPos    = 0;

    if (speakerI2SMutex) xSemaphoreTake(speakerI2SMutex, portMAX_DELAY);
    int16_t silence[64] = {0};
    size_t written;
    i2s_write(SPK_I2S_PORT, silence, sizeof(silence), &written, 10 / portTICK_PERIOD_MS);
    if (speakerI2SMutex) xSemaphoreGive(speakerI2SMutex);
}

bool speakerBuzzerIsActive() {
    return buzzerActive;
}

bool speakerBuzzerUpdate() {
    if (!buzzerActive) return false;
    if (speakerTTSPlaying) return true;   // TTS owns I2S — skip, stay "active"

    if (buzzerPos >= BUZZER_PCM_LEN) {
        LOG_INFO("SPK", "speakerBuzzerUpdate() — PCM finished, stopping");
        speakerBuzzerStop();
        return false;
    }

    // Try to acquire mutex — if TTS has it, skip this iteration
    if (speakerI2SMutex && xSemaphoreTake(speakerI2SMutex, 0) != pdTRUE) {
        return true;
    }

    const TickType_t kTimeout = 10 / portTICK_PERIOD_MS;
    const int chunkSamples = 256;
    int remaining = (int)(BUZZER_PCM_LEN - buzzerPos);
    int count = remaining < chunkSamples ? remaining : chunkSamples;

    // Copy from PROGMEM into a local DRAM buffer before writing to I2S
    int16_t localBuf[256];
    memcpy_P(localBuf, &BUZZER_PCM[buzzerPos], count * sizeof(int16_t));

    size_t written = 0;
    esp_err_t err = i2s_write(SPK_I2S_PORT,
                               localBuf,
                               count * sizeof(int16_t),
                               &written,
                               kTimeout);
    if (err != ESP_OK) {
        LOG_ERROR("SPK", "BuzzerUpdate i2s_write ERROR 0x%x at pos=%u", err, buzzerPos);
    } else if (written < count * sizeof(int16_t)) {
        LOG_WARN("SPK", "BuzzerUpdate partial write %u/%u bytes at pos=%u",
                 written, count * sizeof(int16_t), buzzerPos);
    }

    buzzerPos += count;

    if (speakerI2SMutex) xSemaphoreGive(speakerI2SMutex);
    return true;
}

