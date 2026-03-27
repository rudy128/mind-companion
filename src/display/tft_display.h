// =============================================================
// TFT Display — ILI9341 Screen Management
// This file has all the functions declarations used to control the TFT screen
// =============================================================
#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <Arduino.h>
#include <Adafruit_ILI9341.h>

// Setup the TFT display — call this first in setup()
void tftInit();

// Boot splash: M.I.N.D. COMPANION + WiFi IP (ip e.g. "192.168.1.4" or "No WiFi")
void tftShowBootScreen(const String& ipLine);

// Draw title + section labels (Heart, Temp, Stress, Sleep, Voice)
void tftDrawDashboardLabels();

// Update individual sections (only updates if value changed)
void tftUpdateTime(const char* timeStr);
void tftUpdateHeartRate(int bpm, bool fingerPresent);
void tftUpdateTemperature(bool sensorOk, float celsius);
void tftUpdateSleep(const String& quality);
void tftUpdateStress(const String& level, float gsrValue);
// While stress is High: new random quote from AUDIO_QUOTE_MAP on TFT + matching audio (call e.g. every 30s).
void tftHighStressRefreshRandomQuote();
void tftUpdateEmergency(bool active);
void tftUpdateSpeechStatus(const String& text);
void tftShowListening(bool active);



Adafruit_ILI9341& tftGetDisplay();

#endif // TFT_DISPLAY_H
