// =============================================================
// WiFi Manager
// =============================================================
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

bool wifiConnect();             // blocking connect with retry
bool wifiIsConnected();         // true if associated with AP (may have dead TCP stack)
bool wifiEnsureConnected();     // DNS-probe + force-reconnect if stack is stuck
String wifiGetIP();

#endif // WIFI_MANAGER_H