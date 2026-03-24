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

// Show startup screen with system info
void tftShowBootScreen();

// Draw fixed labels (MPU, Heart, Sleep, Stress)
void tftDrawDashboardLabels();

// Update individual sections (only updates if value changed)
void tftUpdateTime(const char* timeStr);
void tftUpdateHeartRate(int bpm, bool fingerPresent);
void tftUpdateMPU(int x, int y, int z);
void tftUpdateSleep(const String& quality);
void tftUpdateStress(const String& level, float gsrValue);
// While stress is High: new random quote from AUDIO_QUOTE_MAP on TFT + matching audio (call e.g. every 30s).
void tftHighStressRefreshRandomQuote();
void tftUpdateIP(const String& ip);
void tftUpdateEmergency(bool active);
void tftUpdateSpeechStatus(const String& text);
void tftShowListening(bool active);



Adafruit_ILI9341& tftGetDisplay();

#endif 
