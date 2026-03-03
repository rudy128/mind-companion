// =============================================================
// WiFi Manager Implementation
// =============================================================
#include "wifi_manager.h"
#include "../config.h"
#include <WiFi.h>
#include <esp_wifi.h>

// ── Internal reconnect helper ─────────────────────────────
static bool _doReconnect() {
    Serial.println("[WiFi] Force-reconnecting...");
    WiFi.disconnect(true);
    // vTaskDelay instead of delay() — we may be called from a FreeRTOS task
    // and delay() only yields to the scheduler on Core 1 (loop).
    vTaskDelay(pdMS_TO_TICKS(1000));
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
    Serial.println("[WiFi] Reconnected — IP: " + WiFi.localIP().toString());

    // Give the TCP/IP stack a moment to fully initialise before
    // the caller attempts a new TLS connection.
    vTaskDelay(pdMS_TO_TICKS(500));

    // Confirm DNS actually works on the fresh connection
    IPAddress testIP;
    if (WiFi.hostByName("api.openai.com", testIP) != 1) {
        Serial.println("[WiFi] DNS still dead after reconnect — giving up");
        return false;
    }
    Serial.println("[WiFi] DNS OK after reconnect");
    return true;
}

bool wifiConnect() {
    if (WiFi.status() == WL_CONNECTED) return true;

    Serial.print("[WiFi] Connecting to ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < WIFI_RETRY_MAX) {
        delay(500);
        Serial.print(".");
        retry++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Connected! IP: " + WiFi.localIP().toString());
        return true;
    }

    Serial.println("[WiFi] FAILED after " + String(WIFI_RETRY_MAX) + " retries");
    return false;
}

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// Verifies the connection actually works by resolving a known host.
// After an SSL error or TCP abort the driver stays "connected" but DNS
// is dead.  This detects that and force-reconnects before we waste
// another HTTP round-trip.
bool wifiEnsureConnected() {
    // Fast path: not associated at all — just reconnect
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Not connected — reconnecting");
        return _doReconnect();
    }

    // DNS probe: resolve a lightweight known host
    IPAddress testIP;
    bool dnsOk = (WiFi.hostByName("api.openai.com", testIP) == 1);
    if (dnsOk) return true;   // all good

    // DNS failed despite being "connected" — TCP stack is stuck
    Serial.println("[WiFi] DNS probe failed — TCP stack stuck, force-reconnecting");
    return _doReconnect();
}

String wifiGetIP() {
    return WiFi.localIP().toString();
}
