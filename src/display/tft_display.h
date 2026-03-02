// =============================================================
// TFT Display — ILI9341 Screen Management
// =============================================================
#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <Arduino.h>
#include <Adafruit_ILI9341.h>

// Must call first
void tftInit();

// Boot animation
void tftShowBootScreen();

// Draw the persistent dashboard labels
void tftDrawDashboardLabels();

// Update individual sections (only redraws if value changed)
void tftUpdateTime(const char* timeStr);
void tftUpdateHeartRate(int bpm, bool fingerPresent);
void tftUpdateMPU(int x, int y, int z);
void tftUpdateSleep(const String& quality);
void tftUpdateStress(const String& level, float gsrValue);
void tftUpdateIP(const String& ip);
void tftUpdateEmergency(bool active);
void tftUpdateSpeechStatus(const String& text);

// Get the global tft object for advanced use
Adafruit_ILI9341& tftGetDisplay();

#endif // TFT_DISPLAY_H
