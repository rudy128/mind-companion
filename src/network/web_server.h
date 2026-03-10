// =============================================================
// Web Server — REMOVED (replaced by MQTT)
// =============================================================
// The AsyncWebServer has been removed to free ~40-60 KB of SRAM.
// All dashboard communication now goes through MQTT (mqtt_client.h).
// This header remains only for backward compatibility of the
// DashboardState type alias — will be cleaned up later.
// =============================================================
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "mqtt_client.h"

// Backward-compat alias so main.cpp doesn't need a full rewrite yet
typedef MqttDashState DashboardState;

#endif // WEB_SERVER_H
