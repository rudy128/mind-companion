// =============================================================
// WiFi Manager
// This file has functions to connect and check WiFi status.
// =============================================================
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

// Connect to WiFi (keeps trying until connected or retries finish)
bool wifiConnect(); 
      
// Check if ESP32 is connected to WiFi
bool wifiIsConnected();    

// Check if WiFi + internet is working
// If not, it will reconnect automatically
bool wifiEnsureConnected();     

// Get current IP address
String wifiGetIP();

// Tell system if last network action failed or not
// If failed = true → next time it will check internet (DNS)
// If false -> no extra check needed
void wifiSetLastCycleFailed(bool failed);

#endif 