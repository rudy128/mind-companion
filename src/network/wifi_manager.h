// =============================================================
// WiFi Manager
// =============================================================
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

bool wifiConnect();             // blocking connect with retry
bool wifiIsConnected();
String wifiGetIP();

#endif // WIFI_MANAGER_H
