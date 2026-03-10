// =============================================================
// WiFi Manager Implementation
// =============================================================
#include "wifi_manager.h"
#include "../config.h"
#include <WiFi.h>
#include <esp_wifi.h>

// Count consecutive network failures. Only force-reconnect after
// several failures in a row — a single SSL timeout doesn't mean
// the WiFi stack is dead.
static int _consecutiveFailures = 0;
static const int FAILURE_THRESHOLD = 3;   // need 3 failures before forcing reconnect

void wifiSetLastCycleFailed(bool failed) {
    if (failed) {
        _consecutiveFailures++;
    } else {
        _consecutiveFailures = 0;
    }
}

// ── Internal reconnect helper ─────────────────────────────
static bool _doReconnect() {
    Serial.println("[WiFi] Force-reconnecting...");
    WiFi.disconnect(true);
    vTaskDelay(pdMS_TO_TICKS(500));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < WIFI_RETRY_MAX) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
        retry++;
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Reconnect FAILED");
        return false;
    }
    Serial.printf("[WiFi] Reconnected — IP: %s\n", WiFi.localIP().toString().c_str());
    vTaskDelay(pdMS_TO_TICKS(300));
    _consecutiveFailures = 0;
    return true;
}

bool wifiConnect() {
    if (WiFi.status() == WL_CONNECTED) return true;

    Serial.printf("[WiFi] Connecting to %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);            // ensure station-only mode
    WiFi.disconnect(true);           // clear any stale state
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < WIFI_RETRY_MAX) {
        delay(500);
        Serial.print(".");
        retry++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.printf("[WiFi] FAILED after %d retries\n", WIFI_RETRY_MAX);
    return false;
}

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// Light-weight connection check.
// - If WiFi is actually disconnected → reconnect immediately.
// - If WiFi is connected but we've had several consecutive SSL failures →
//   do a DNS probe. Only if DNS also fails do we force-reconnect.
// - Otherwise, trust that WiFi is fine. A single SSL fail is normal
//   (server timeout, network blip) and doesn't warrant tearing down
//   the entire WiFi stack which kills the web server and camera.
bool wifiEnsureConnected() {
    // Not associated at all — reconnect
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Not connected — reconnecting");
        return _doReconnect();
    }

    // WiFi is connected — only probe DNS if we've had repeated failures
    if (_consecutiveFailures < FAILURE_THRESHOLD) {
        return true;   // trust it's fine
    }

    // Multiple consecutive failures — check DNS
    Serial.printf("[WiFi] %d consecutive failures — DNS probe...\n", _consecutiveFailures);
    IPAddress testIP;
    if (WiFi.hostByName("api.openai.com", testIP) == 1) {
        // DNS works — the failures are server-side, not our WiFi
        Serial.println("[WiFi] DNS OK — failures are upstream, not WiFi");
        _consecutiveFailures = 0;
        return true;
    }

    // DNS failed despite being "connected" — TCP stack is stuck
    Serial.println("[WiFi] DNS probe failed — force-reconnecting");
    return _doReconnect();
}

String wifiGetIP() {
    return WiFi.localIP().toString();
}
