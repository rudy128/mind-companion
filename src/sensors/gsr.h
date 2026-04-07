// =============================================================
// GSR Sensor — Galvanic Skin Response
// =============================================================
#ifndef GSR_SENSOR_H
#define GSR_SENSOR_H

#include <Arduino.h>

// Initialize GSR sensor
void   gsrInit();

// Read GSR sensor and return scaled conductance (0–50)
// 0 = not worn, 0–15 Low, 15–30 Moderate, 30–50 High
float  gsrReadConductance();

// Convert scaled conductance to a stress level string
String gsrGetStressLevel(float scaledValue);   

#endif 
