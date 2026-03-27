// =============================================================
// Microphone (INMP441)
// Records audio using I2S and prepares it for processing
// =============================================================
#include "microphone.h"
#include "../config.h"
#include "../network/logger.h"
#include "driver/i2s.h"

// Track if I2S driver is already installed
static bool _driverInstalled = false;

// Install I2S driver
static bool _installDriver() {
    if (_driverInstalled) return true;        // Already installed

    // I2S configuration
    i2s_config_t i2s_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = MIC_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,  
        .intr_alloc_flags     = 0,
        .dma_buf_count        = 8,
        .dma_buf_len          = 1024,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0
    };


    // I2S pin configuration for INMP441
    i2s_pin_config_t pin_config = {
        .bck_io_num   = MIC_I2S_SCK,
        .ws_io_num    = MIC_I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = MIC_I2S_SD
    };

    // Install I2S driver
    esp_err_t err = i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        LOG_ERROR("MIC", "i2s_driver_install FAILED: 0x%x (%s)", err, esp_err_to_name(err));
        return false;
    }
    
    // Set I2S pins
    err = i2s_set_pin(MIC_I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        LOG_ERROR("MIC", "i2s_set_pin FAILED: 0x%x (%s)", err, esp_err_to_name(err));
        i2s_driver_uninstall(MIC_I2S_PORT);
        return false;
    }

    _driverInstalled = true;
    return true;
}

// Uninstall I2S driver 
static void _uninstallDriver() {
    if (!_driverInstalled) return;
    i2s_driver_uninstall(MIC_I2S_PORT);
    _driverInstalled = false;
}

size_t micRecord(int16_t* buffer, size_t bufferSize) {
    _uninstallDriver();   // ensure clean slate
    if (!_installDriver()) {
        LOG_ERROR("MIC", "Cannot install I2S driver — recording aborted");
        return 0;
    }

    size_t bytesRead = 0;
    esp_err_t err = i2s_read(MIC_I2S_PORT, buffer, bufferSize, &bytesRead, portMAX_DELAY);
    
    if (err != ESP_OK) {
        LOG_ERROR("MIC", "i2s_read error: 0x%x (%s)", err, esp_err_to_name(err));
        _uninstallDriver();
        return 0;
    }

    // Check audio levels 
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
