// =============================================================
// GSR Sensor — Galvanic Skin Response
// =============================================================
#ifndef GSR_SENSOR_H
#define GSR_SENSOR_H

#include <Arduino.h>

// Initialize GSR sensor
void   gsrInit();

// Read GSR sensor and return skin conductance in µS
// Higher conductance → more sweat → higher stress
float  gsrReadConductance();

// Convert conductance (µS) to a stress level string
String gsrGetStressLevel(float conductance);   

#endif 
