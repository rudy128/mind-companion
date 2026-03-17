// =============================================================
// MQTT Client — Lightweight PubSub for M.I.N.D. Companion
// =============================================================
// Replaces ESPAsyncWebServer. The ESP32 maintains ONE TCP
// connection to a local Mosquitto broker. The Next.js dashboard
// subscribes via MQTT-over-WebSocket on the same broker.
//
// Runs on Core 0 as a dedicated FreeRTOS task so that any
// blocking socket reads never stall the sensor loop on Core 1.
//
// Publish:  mind/data   — sensor telemetry JSON  (1 Hz)
//           mind/alert  — emergency flag changes
// Subscribe: mind/cmd   — dashboard commands (JSON {cmd:"..."})
// =============================================================
#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <freertos/semphr.h>

// Shared state struct — replaces DashboardState from web_server.h
// Populated by main.cpp (Core 1), read by MQTT task (Core 0).
// Individual fields are atomic-width or tolerate tearing, so no
// mutex is needed for the telemetry path.
struct MqttDashState {
    float  gsrValue;
    char   stressLevel[16];
    int    heartBPM;
    bool   fingerPresent;
    char   sleepQuality[16];
    bool   emergencyActive;
    float  accelX, accelY, accelZ;
    float  temperature;
    char   lastVoiceCommand[128];
    bool   breathingActive;
    bool   cameraOpen;
    bool   micActive;
};

// ── Lifecycle ────────────────────────────────────────────────

// Call once in setup() after WiFi is connected.
// Spawns the MQTT task on Core 0. Pass a pointer to the shared state.
void mqttInit(MqttDashState* state);

// ── Manual publish helpers (thread-safe, can be called from Core 1) ──

// Force-publish the current state immediately
void mqttPublishState();

// Publish emergency alert change to mind/alert
void mqttPublishAlert(bool active);

// ── Command callback registration ───────────────────────────
// Commands received on mind/cmd are JSON: {"cmd":"breathe"}, {"cmd":"alarm_on"}, etc.
// NOTE: The callback runs on Core 0.  Keep it short or use a queue to
// defer work to Core 1.
typedef void (*MqttCmdCallback)(const String& cmd);
void mqttSetCommandCallback(MqttCmdCallback cb);

// ── Status ───────────────────────────────────────────────────
bool mqttIsConnected();

// Mutex for thread-safe access to MqttDashState.
// Created in mqttInit(). Core 1 takes it while writing dashState fields,
// Core 0 MQTT task takes it while copying fields for JSON serialization.
extern SemaphoreHandle_t mqttDashMutex;

#endif // MQTT_CLIENT_H