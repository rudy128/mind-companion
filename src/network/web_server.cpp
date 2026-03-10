// =============================================================
// Web Server — REMOVED (replaced by MQTT)
// =============================================================
// The ESPAsyncWebServer and all HTTP endpoints have been removed
// to free ~40-60 KB of SRAM on the ESP32-S3.
//
// Dashboard communication is now handled by mqtt_client.cpp:
//   • Sensor data published to  mind/data   (1 Hz)
//   • Commands received from    mind/cmd
//   • Alerts published to       mind/alert
//
// The camera MJPEG stream remains on port 81 via esp_http_server
// (handled in camera.cpp) — this is a separate lightweight server
// that only runs when a client connects.
//
// This file is intentionally empty. It will be removed in a
// future cleanup pass.
// =============================================================
