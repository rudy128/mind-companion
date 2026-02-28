// =============================================================
// Camera — ESP32-S3 Built-in OV2640
// =============================================================
#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include <Arduino.h>
#include "esp_camera.h"

bool cameraInit();

// Capture a single JPEG frame.  Caller must call esp_camera_fb_return(fb) after use.
camera_fb_t* cameraCapture();

// Helper: capture frame directly into a buffer for async streaming.
// Returns number of bytes written, or 0 on failure.
size_t cameraCaptureToBuffer(uint8_t* buffer, size_t maxLen);

#endif // CAMERA_MODULE_H
