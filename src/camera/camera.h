// =============================================================
// Camera — ESP32-S3 Built-in OV2640
// =============================================================
#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include <Arduino.h>
#include "esp_camera.h"

// Initialise the OV2640 sensor. Call once in setup().
bool cameraInit();

// Start the MJPEG stream server on port 81.
// After calling this, the stream is available at http://<IP>:81/stream
// Uses esp_http_server (not AsyncWebServer) so the streaming loop
// runs independently and never blocks the main web server.
void cameraStartStreamServer();

#endif // CAMERA_MODULE_H
