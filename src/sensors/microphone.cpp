// =============================================================
// Microphone — INMP441 I2S Implementation
// =============================================================
#include "microphone.h"
#include "../config.h"
#include "driver/i2s.h"

void micInit() {
    Serial.println("[MIC] Initializing I2S #0 (RX)...");

    i2s_config_t i2s_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = MIC_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
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

    i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(MIC_I2S_PORT, &pin_config);
    Serial.println("[MIC] I2S #0 OK");
}

size_t micRecord(int16_t* buffer, size_t bufferSize) {
    // A single i2s_read call may return far fewer bytes than requested
    // (DMA buffer boundary, 100ms window, etc.).  Loop until the full
    // buffer is filled or a per-chunk timeout fires too many times.
    size_t totalRead = 0;
    const uint8_t MAX_RETRIES = 40;   // 40 × 100 ms = 4 s max — covers 2 s audio + latency
    uint8_t retries = 0;

    while (totalRead < bufferSize && retries < MAX_RETRIES) {
        size_t bytesRead = 0;
        i2s_read(MIC_I2S_PORT,
                 (uint8_t*)buffer + totalRead,
                 bufferSize - totalRead,
                 &bytesRead,
                 pdMS_TO_TICKS(100));
        if (bytesRead > 0) {
            totalRead += bytesRead;
        } else {
            retries++;
        }
        // Feed the task watchdog while we block here so Core 0 IDLE stays happy
        vTaskDelay(1);
    }
    return totalRead;
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
