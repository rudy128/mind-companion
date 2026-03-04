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

// Tell the WiFi manager that the last network operation failed/succeeded.
// When true, wifiEnsureConnected() will do a DNS probe on the next call.
// When false (default after successful reconnect), it skips the probe.
void wifiSetLastCycleFailed(bool failed);

#endif // WIFI_MANAGER_H