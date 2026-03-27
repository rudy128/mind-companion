// =============================================================
// WiFi Manager
// Handles WiFi connection and reconnection
// =============================================================
#include "wifi_manager.h"
#include "../config.h"
#include <WiFi.h>

// Count how many times network failed in a row
static int _consecutiveFailures = 0;
static const int FAILURE_THRESHOLD = 3;   // need 3 failures before forcing reconnect

// Track if last network operation failed or not
void wifiSetLastCycleFailed(bool failed) {
    if (failed) {
        _consecutiveFailures++;
    } else {
        _consecutiveFailures = 0;     //Reset if successful
    }
}

// ── Force reconnect to WiFi ─────────────────────────────
static bool _doReconnect() {
    Serial.println("[WiFi] Force-reconnecting...");
    WiFi.disconnect(true);           // reset Wi-Fi state
    vTaskDelay(pdMS_TO_TICKS(500));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retry = 0;
    //Try to connect multiple times
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

// Connect to Wi-Fi 
bool wifiConnect() {
    if (WiFi.status() == WL_CONNECTED) return true;  // Already connected

    Serial.printf("[WiFi] Connecting to %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);            //station mode only
    WiFi.disconnect(true);          // clear old connection
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retry = 0;

    // Try connecting
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

// Check if connected
bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// Advanced Wi-Fi Connection check
bool wifiEnsureConnected() {
    // Not connected at all — reconnect
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Not connected — reconnecting");
        return _doReconnect();
    }

    // If only few failures , assume Wi-Fi is okay
    if (_consecutiveFailures < FAILURE_THRESHOLD) {
        return true;   
    }

    // Too many Failures — check DNS
    Serial.printf("[WiFi] %d consecutive failures — DNS probe...\n", _consecutiveFailures);
    IPAddress testIP;
    if (WiFi.hostByName("api.openai.com", testIP) == 1) {
        // DNS works — Wi-fi is fine
        Serial.println("[WiFi] DNS OK — failures are upstream, not WiFi");
        _consecutiveFailures = 0;
        return true;
    }

    // DNS failed - Reconnect Wi-Fi
    Serial.println("[WiFi] DNS probe failed — force-reconnecting");
    return _doReconnect();
}

// Get Device IP
String wifiGetIP() {
    return WiFi.localIP().toString();
}
