// =============================================================
// Web Server — AsyncWebServer Routes + Dashboard
// =============================================================
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

// Shared state struct for dashboard display
struct DashboardState {
    float  gsrOhms;
    String stressLevel;
    int    heartBPM;
    bool   fingerPresent;
    String sleepQuality;
    bool   emergencyActive;
    float  accelX, accelY, accelZ;
    float  temperature;
    String lastVoiceCommand;
    bool   breathingActive;
};

// Initialize and start the async web server on port 80.
// Pass a pointer to a DashboardState that the server reads each request.
void webServerInit(DashboardState* state);

// Register callbacks for dashboard control buttons.
// Any callback left as nullptr will be ignored.
void webServerSetCallbacks(
    void (*breathe)(),
    void (*alarmOn)(),
    void (*alarmOff)(),
    void (*vibrate)()
);

#endif // WEB_SERVER_H
