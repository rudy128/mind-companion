// =============================================================
// Camera — ESP32-S3 Built-in OV2640 Implementation
// MJPEG stream served on port 81 via esp_http_server.
// This keeps streaming fully independent of AsyncWebServer so
// the infinite frame loop never blocks dashboard / API requests.
// =============================================================
#include "camera.h"
#include "../config.h"
#include "esp_http_server.h"
#include "esp_timer.h"

// ── MJPEG multipart constants ─────────────────────────────
static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=frame";
static const char* STREAM_BOUNDARY = "\r\n--frame\r\n";
static const char* STREAM_PART     =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static httpd_handle_t _streamServer = nullptr;

// ── Stream handler — infinite loop, one JPEG per iteration ──
static esp_err_t streamHandler(httpd_req_t* req) {
    camera_fb_t* fb  = nullptr;
    esp_err_t    res = ESP_OK;
    char         partBuf[64];

    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    // Allow cross-origin access so the dashboard page (same IP, port 80)
    // can load the stream from port 81 without CORS errors.
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("[CAM] Frame capture failed");
            res = ESP_FAIL;
            break;
        }

        // Boundary
        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if (res != ESP_OK) { esp_camera_fb_return(fb); break; }

        // Part header
        size_t hlen = snprintf(partBuf, sizeof(partBuf), STREAM_PART, fb->len);
        res = httpd_resp_send_chunk(req, partBuf, hlen);
        if (res != ESP_OK) { esp_camera_fb_return(fb); break; }

        // JPEG data
        res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
        esp_camera_fb_return(fb);
        if (res != ESP_OK) break;
    }

    return res;
}

// ── Start the dedicated stream server on port 81 ─────────
void cameraStartStreamServer() {
    if (_streamServer) return;   // already running

    httpd_config_t config  = HTTPD_DEFAULT_CONFIG();
    config.server_port     = 81;
    config.ctrl_port       = 32769;   // avoid clash with AsyncWebServer's ctrl port
    config.max_uri_handlers = 4;

    httpd_uri_t streamUri = {
        .uri      = "/stream",
        .method   = HTTP_GET,
        .handler  = streamHandler,
        .user_ctx = nullptr
    };

    if (httpd_start(&_streamServer, &config) == ESP_OK) {
        httpd_register_uri_handler(_streamServer, &streamUri);
        Serial.println("[CAM] Stream server started — http://<IP>:81/stream");
    } else {
        Serial.println("[CAM] ERROR: failed to start stream server on port 81");
    }
}

// ── Camera sensor init ────────────────────────────────────
bool cameraInit() {
    Serial.println("[CAM] Initializing OV2640...");

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_1;   // channel 1 (0 = LED PWM)
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

    config.xclk_freq_hz = 20000000;
    config.pixel_format  = PIXFORMAT_JPEG;
    config.frame_size    = FRAMESIZE_QVGA;   // 320×240 — lighter than VGA
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
