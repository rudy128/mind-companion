// =============================================================
// MQTT Client — PubSubClient on Core 0 (FreeRTOS task)
// =============================================================
// Runs entirely on Core 0 so that any socket blocking never
// stalls the sensor / display loop on Core 1.
//
// The task wakes every 50 ms:
//   • calls _mqtt.loop()   — keepalive pings + incoming msgs
//   • every 1 s publishes the shared dashState to mind/data
//   • reconnects automatically on drop
//
// Core 1 (main loop) writes to MqttDashState without a mutex.
// Individual fields are small enough that tearing is harmless
// for telemetry.  The mqttPublishAlert() helper is guarded by
// a simple volatile flag checked inside the task.
// =============================================================
#include "mqtt_client.h"
#include "../config.h"
#include "logger.h"
#include "wifi_manager.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ── Internal state ───────────────────────────────────────────
static WiFiClient       _wifiClient;
static PubSubClient     _mqtt(_wifiClient);
static MqttDashState*   _state          = nullptr;
static MqttCmdCallback  _cmdCallback    = nullptr;
static unsigned long    _lastPublish    = 0;
static unsigned long    _lastReconnect  = 0;
static const unsigned long PUBLISH_INTERVAL_MS   = 1000;   // 1 Hz telemetry
static const unsigned long RECONNECT_INTERVAL_MS = 2000;   // retry every 2 s
static const unsigned long TASK_TICK_MS          = 50;      // task wakes every 50 ms

// Task handle
static TaskHandle_t     _mqttTaskHandle = nullptr;

// Cross-core alert request (set by Core 1, consumed by Core 0 task)
static volatile bool    _alertPending   = false;
static volatile bool    _alertValue     = false;

// DashState mutex — protects String fields from tearing across cores
SemaphoreHandle_t mqttDashMutex = nullptr;

// ── MQTT message callback (incoming commands) ────────────────
static void _onMessage(char* topic, byte* payload, unsigned int length) {
    char buf[256];
    unsigned int copyLen = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;
    memcpy(buf, payload, copyLen);
    buf[copyLen] = '\0';

    LOG_INFO("MQTT", "Received [%s]: %s", topic, buf);

    if (String(topic) == MQTT_TOPIC_CMD && _cmdCallback) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, buf);
        if (err) {
            LOG_WARN("MQTT", "CMD parse error: %s", err.c_str());
            return;
        }
        String cmd = doc["cmd"] | "";
        if (cmd.length() > 0) {
            LOG_INFO("MQTT", "Command: %s", cmd.c_str());
            _cmdCallback(cmd);
        }
    }
}

// ── Connect / reconnect to broker ────────────────────────────
static bool _connect() {
    LOG_INFO("MQTT", "Connecting to %s:%d ...", MQTT_SERVER, MQTT_PORT);
    
    if (_mqtt.connect(MQTT_CLIENT_ID)) {
        LOG_INFO("MQTT", "Connected to broker");
        _mqtt.subscribe(MQTT_TOPIC_CMD);
        LOG_INFO("MQTT", "Subscribed to %s", MQTT_TOPIC_CMD);
        return true;
    }

    LOG_WARN("MQTT", "Connection failed, rc=%d", _mqtt.state());
    return false;
}

// ── Publish helpers (called from the task context) ───────────

static void _publishState() {
    if (!_state || !_mqtt.connected()) return;

    // Snapshot the shared state under mutex — hold time is just the
    // struct copy, not the JSON serialization or network I/O.
    MqttDashState snap;
    if (mqttDashMutex && xSemaphoreTake(mqttDashMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        snap = *_state;
        // Clear one-shot camera flag while we hold the mutex
        if (_state->cameraOpen) {
            _state->cameraOpen = false;
        }
        xSemaphoreGive(mqttDashMutex);
    } else {
        LOG_WARN("MQTT", "_publishState skipped — dashMutex timeout");
        return;
    }

    // DEBUG: unconditionally log every publish
    Serial.printf("[PUB] bpm=%d finger=%d gsr=%.1f\n",
                  snap.heartBPM, (int)snap.fingerPresent,
                  (float)snap.gsrValue);

    JsonDocument doc;
    doc["bpm"]       = snap.heartBPM;
    doc["finger"]    = snap.fingerPresent;
    doc["gsr"]       = snap.gsrValue;
    doc["stress"]    = snap.stressLevel;
    doc["sleep"]     = snap.sleepQuality;
    doc["emergency"] = snap.emergencyActive;
    doc["ax"]        = serialized(String(snap.accelX, 2));
    doc["ay"]        = serialized(String(snap.accelY, 2));
    doc["az"]        = serialized(String(snap.accelZ, 2));
    doc["temp"]      = serialized(String(snap.temperature, 1));
    doc["voice"]     = snap.lastVoiceCommand;
    doc["breathing"] = snap.breathingActive;
    doc["camOpen"]   = snap.cameraOpen;
    doc["ip"]        = WiFi.localIP().toString();
    doc["heap"]      = esp_get_free_heap_size();
    doc["uptime"]    = millis() / 1000;

    char json[512];
    size_t len = serializeJson(doc, json, sizeof(json));
    bool ok = _mqtt.publish(MQTT_TOPIC_DATA, (const uint8_t*)json, (unsigned int)len, false);
    if (!ok) {
        LOG_WARN("MQTT", "publish(mind/data) failed — connected=%d", _mqtt.connected());
    }
}

static void _publishAlert(bool active) {
    if (!_mqtt.connected()) return;

    JsonDocument doc;
    doc["emergency"] = active;
    doc["ts"]        = millis();

    char json[64];
    size_t len = serializeJson(doc, json, sizeof(json));
    _mqtt.publish(MQTT_TOPIC_ALERT, json, true);   // retained
}

// ── The MQTT FreeRTOS task — runs on Core 0 ──────────────────
static void _mqttTask(void* param) {
    LOG_INFO("MQTT", "Task started on Core %d", xPortGetCoreID());

    for (;;) {
        // ── Reconnect if needed ──────────────────────────
        if (!_mqtt.connected()) {
            unsigned long now = millis();
            if (now - _lastReconnect >= RECONNECT_INTERVAL_MS) {
                _lastReconnect = now;
                LOG_WARN("MQTT", "Not connected (state=%d) — reconnecting...", _mqtt.state());
                if (_connect()) {
                    _mqtt.loop();          // drain SUBACK
                    _lastPublish = millis(); // publish immediately after reconnect
                }
            }
            vTaskDelay(pdMS_TO_TICKS(TASK_TICK_MS));
            continue;
        }

        // ── Process incoming messages + keepalive ────────
        unsigned long t0 = millis();
        bool ok = _mqtt.loop();
        unsigned long dt = millis() - t0;
        if (dt > 100) {
            LOG_WARN("MQTT", "_mqtt.loop() blocked for %lu ms", dt);
        }
        if (!ok) {
            LOG_WARN("MQTT", "loop() returned false (state=%d, %lu ms) — will reconnect",
                     _mqtt.state(), dt);
            vTaskDelay(pdMS_TO_TICKS(TASK_TICK_MS));
            continue;
        }

        // ── Consume cross-core alert request ─────────────
        if (_alertPending) {
            _publishAlert(_alertValue);
            _alertPending = false;
        }

        // ── Publish telemetry at 1 Hz ────────────────────
        unsigned long now = millis();
        if (now - _lastPublish >= PUBLISH_INTERVAL_MS) {
            _lastPublish = now;
            _publishState();
        }

        // ── Debug heartbeat every 10 s ───────────────────
        static unsigned long _dbgLast = 0;
        static unsigned long _dbgTicks = 0;
        _dbgTicks++;
        if (now - _dbgLast >= 10000UL) {
            LOG_INFO("MQTT", "task ticks=%lu connected=%d state=%d heap=%lu",
                     _dbgTicks, _mqtt.connected(), _mqtt.state(),
                     esp_get_free_heap_size());
            _dbgTicks = 0;
            _dbgLast = now;
        }

        // ── Sleep 50 ms — gives ~20 loop() calls per second ─
        vTaskDelay(pdMS_TO_TICKS(TASK_TICK_MS));
    }
}

// ── Public API ───────────────────────────────────────────────

void mqttInit(MqttDashState* state) {
    _state = state;

    // Create dashState mutex once
    if (!mqttDashMutex) {
        mqttDashMutex = xSemaphoreCreateMutex();
    }

    _mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    _mqtt.setCallback(_onMessage);
    _mqtt.setBufferSize(1024);
    _mqtt.setKeepAlive(60);
    _mqtt.setSocketTimeout(2);     // 2 s — OK because we're on our own core now
    _wifiClient.setTimeout(500);   // 500 ms — won't stall sensors

    _connect();
    _mqtt.loop();   // drain SUBACK

    LOG_INFO("MQTT", "Spawning MQTT task on Core 0 (keepalive=60s, tick=%lums)",
             TASK_TICK_MS);

    xTaskCreatePinnedToCore(
        _mqttTask,          // function
        "mqtt",             // name
        8192,               // stack (8 KB — plenty for JSON + PubSubClient)
        nullptr,            // param
        2,                  // priority (above idle, below WiFi event task)
        &_mqttTaskHandle,   // handle
        0                   // ← Core 0
    );
}

void mqttPublishState() {
    // Can still be called from Core 1 for a forced immediate publish.
    // The task does its own 1 Hz publish, but this is safe because
    // PubSubClient serialises writes internally.
    _publishState();
}

void mqttPublishLog(const char* level, const char* tag, const char* msg) {
    if (!_mqtt.connected()) return;

    JsonDocument doc;
    doc["ts"]    = millis();
    doc["level"] = level;
    doc["tag"]   = tag;
    doc["msg"]   = msg;

    char json[256];
    size_t len = serializeJson(doc, json, sizeof(json));
    _mqtt.publish(MQTT_TOPIC_LOGS, json, len);
}

void mqttPublishAlert(bool active) {
    // Called from Core 1 — hand off to the task via volatile flag
    // so the actual publish happens on Core 0 where the socket lives.
    _alertValue   = active;
    _alertPending = true;
}

void mqttSetCommandCallback(MqttCmdCallback cb) {
    _cmdCallback = cb;
}

bool mqttIsConnected() {
    return _mqtt.connected();
}