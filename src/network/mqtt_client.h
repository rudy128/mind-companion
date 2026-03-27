// =============================================================
// MQTT Client
// This file defines how ESP32 sends and receives data using MQTT.
// =============================================================
#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <freertos/semphr.h>

// Shared data between main loop (Core 1) and MQTT task (Core 0)
// Main loop updates values, MQTT sends them to server every second
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

// Call once in setup() after WiFi is connected.
void mqttInit(MqttDashState* state);

// ── Send data manually ──

// Send emergency alert
void mqttPublishAlert(bool active);

// Receieve commands from dashboard
typedef void (*MqttCmdCallback)(const String& cmd);

// Function to handle received commands
void mqttSetCommandCallback(MqttCmdCallback cb);

// Check MQTT connection status
bool mqttIsConnected();

// Mutex to share data safely between cores
extern SemaphoreHandle_t mqttDashMutex;

#endif