// =============================================================
// WiFi Manager Implementation
// =============================================================
#include "wifi_manager.h"
#include "../config.h"
#include <WiFi.h>

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

String wifiGetIP() {
    return WiFi.localIP().toString();
}
