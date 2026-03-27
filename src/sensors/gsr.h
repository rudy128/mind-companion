// =============================================================
// GSR Sensor — Galvanic Skin Response
// =============================================================
#ifndef GSR_SENSOR_H
#define GSR_SENSOR_H

#include <Arduino.h>

// Initialize GSR sensor
void   gsrInit();

// Read GSR sensor and calculate conductance (µS)
// Lower resistance → more sweat → higher stress
float  gsrReadConductance();  

// Convert conductance value to a stress level string
String gsrGetStressLevel(float c);   

#endif 
