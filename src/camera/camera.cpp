// =============================================================
// Camera — ESP32-S3 Built-in OV2640 Implementation
// =============================================================
#include "camera.h"
#include "../config.h"

bool cameraInit() {
    Serial.println("[CAM] Initializing OV2640...");

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_1;   // channel 1 (channel 0 = LED PWM)
    config.ledc_timer   = LEDC_TIMER_1;

    config.pin_d0     = CAM_PIN_D0;
    config.pin_d1     = CAM_PIN_D1;
    config.pin_d2     = CAM_PIN_D2;
    config.pin_d3     = CAM_PIN_D3;
    config.pin_d4     = CAM_PIN_D4;
    config.pin_d5     = CAM_PIN_D5;
    config.pin_d6     = CAM_PIN_D6;
    config.pin_d7     = CAM_PIN_D7;

    config.pin_xclk    = CAM_PIN_XCLK;
    config.pin_pclk    = CAM_PIN_PCLK;
    config.pin_vsync   = CAM_PIN_VSYNC;
    config.pin_href    = CAM_PIN_HREF;

    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;

    config.pin_pwdn    = CAM_PIN_PWDN;
    config.pin_reset   = CAM_PIN_RESET;

    config.xclk_freq_hz = 20000000;  // 20 MHz (standard for Freenove ESP32-S3 CAM)
    config.pixel_format  = PIXFORMAT_JPEG;
    config.frame_size    = FRAMESIZE_QVGA;   // 320×240
    config.jpeg_quality  = 12;
    config.fb_count      = 2;
    config.grab_mode     = CAMERA_GRAB_LATEST;
    config.fb_location   = CAMERA_FB_IN_PSRAM;

    if (!psramFound()) {
        Serial.println("[CAM] No PSRAM — using DRAM (1 buffer)");
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.fb_count    = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAM] ERROR: init failed 0x%x\n", err);
        return false;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_vflip(s, 0);
        s->set_brightness(s, 1);
        s->set_saturation(s, 0);
    }

    Serial.println("[CAM] OV2640 OK");
    return true;
}

camera_fb_t* cameraCapture() {
    return esp_camera_fb_get();
}

size_t cameraCaptureToBuffer(uint8_t* buffer, size_t maxLen) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return 0;

    // Build MJPEG boundary header
    const char* boundary = "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ";
    char header[128];
    int headerLen = snprintf(header, sizeof(header), "%s%zu\r\n\r\n", boundary, fb->len);

    if ((size_t)headerLen + fb->len > maxLen) {
        esp_camera_fb_return(fb);
        return 0;  // frame too large for buffer
    }

    memcpy(buffer, header, headerLen);
    memcpy(buffer + headerLen, fb->buf, fb->len);
    size_t total = headerLen + fb->len;

    esp_camera_fb_return(fb);
    return total;
}
