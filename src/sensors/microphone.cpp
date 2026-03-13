// =============================================================
// Microphone — INMP441 I2S Implementation
// =============================================================
// Uses exact same I2S config as the working standalone script.
// Time-sliced with speaker on I2S_NUM_0.
// =============================================================
#include "microphone.h"
#include "../config.h"
#include "../network/logger.h"
#include "driver/i2s.h"

// ── Internal helpers ─────────────────────────────────────────
static bool _driverInstalled = false;

static bool _installDriver() {
    if (_driverInstalled) return true;

    // EXACT config from working script - DO NOT CHANGE
    i2s_config_t i2s_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = MIC_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,  // ← KEY CHANGE from working script
        .intr_alloc_flags     = 0,
        .dma_buf_count        = 8,
        .dma_buf_len          = 1024,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num   = MIC_I2S_SCK,
        .ws_io_num    = MIC_I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = MIC_I2S_SD
    };

    esp_err_t err = i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        LOG_ERROR("MIC", "i2s_driver_install FAILED: 0x%x (%s)", err, esp_err_to_name(err));
        return false;
    }

    err = i2s_set_pin(MIC_I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        LOG_ERROR("MIC", "i2s_set_pin FAILED: 0x%x (%s)", err, esp_err_to_name(err));
        i2s_driver_uninstall(MIC_I2S_PORT);
        return false;
    }

    _driverInstalled = true;
    return true;
}

static void _uninstallDriver() {
    if (!_driverInstalled) return;
    i2s_driver_uninstall(MIC_I2S_PORT);
    _driverInstalled = false;
}

// ── Public API ───────────────────────────────────────────────

void micInit() {
    LOG_INFO("MIC", "Initializing I2S #%d (RX) — SCK=%d WS=%d SD=%d rate=%d",
             (int)MIC_I2S_PORT, MIC_I2S_SCK, MIC_I2S_WS, MIC_I2S_SD, MIC_SAMPLE_RATE);

    // Do a quick install/uninstall just to verify the pins & config are valid
    if (_installDriver()) {
        // Read & discard a tiny chunk to prime the DMA — proves RX works
        int16_t dummy[64];
        size_t n = 0;
        i2s_read(MIC_I2S_PORT, dummy, sizeof(dummy), &n, pdMS_TO_TICKS(200));
        LOG_INFO("MIC", "I2S #%d RX verified (%u bytes in probe read)", (int)MIC_I2S_PORT, n);
        _uninstallDriver();   // release until the first real recording
    } else {
        LOG_ERROR("MIC", "I2S init FAILED — microphone will not work");
    }
}

size_t micRecord(int16_t* buffer, size_t bufferSize) {
    // ── Install driver fresh for this recording session ──
    _uninstallDriver();   // ensure clean slate
    if (!_installDriver()) {
        LOG_ERROR("MIC", "Cannot install I2S driver — recording aborted");
        return 0;
    }

    // ── Record using SAME method as working script ──
    // Single blocking i2s_read call with portMAX_DELAY (like working script)
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(MIC_I2S_PORT, buffer, bufferSize, &bytesRead, portMAX_DELAY);
    
    if (err != ESP_OK) {
        LOG_ERROR("MIC", "i2s_read error: 0x%x (%s)", err, esp_err_to_name(err));
        _uninstallDriver();
        return 0;
    }

    // ── Analyze audio levels for debugging ──
    if (bytesRead > 0) {
        int16_t minVal = 32767, maxVal = -32768;
        int64_t sum = 0;
        size_t samples = bytesRead / sizeof(int16_t);
        for (size_t i = 0; i < samples; i++) {
            int16_t s = buffer[i];
            if (s < minVal) minVal = s;
            if (s > maxVal) maxVal = s;
            sum += abs(s);
        }
        int32_t avgLevel = (int32_t)(sum / samples);
        LOG_INFO("MIC", "Audio: min=%d max=%d avg=%d peak-to-peak=%d", 
                 minVal, maxVal, avgLevel, maxVal - minVal);
        
        if ((maxVal - minVal) < 500) {
            LOG_WARN("MIC", "Audio appears SILENT - check mic wiring!");
        }
    }

    // ── Release the I2S driver ──
    _uninstallDriver();

    return bytesRead;
}

void micCreateWavHeader(uint8_t* wav, size_t pcmSize, uint32_t sampleRate) {
    uint32_t fileSize = pcmSize + 36;
    memcpy(wav,      "RIFF", 4);
    *(uint32_t*)(wav + 4)  = fileSize;
    memcpy(wav + 8,  "WAVEfmt ", 8);
    *(uint32_t*)(wav + 16) = 16;              // fmt chunk size
    *(uint16_t*)(wav + 20) = 1;               // PCM
    *(uint16_t*)(wav + 22) = 1;               // mono
    *(uint32_t*)(wav + 24) = sampleRate;
    *(uint32_t*)(wav + 28) = sampleRate * 2;  // byte rate (16-bit mono)
    *(uint16_t*)(wav + 32) = 2;               // block align
    *(uint16_t*)(wav + 34) = 16;              // bits per sample
    memcpy(wav + 36, "data", 4);
    *(uint32_t*)(wav + 40) = pcmSize;
}
