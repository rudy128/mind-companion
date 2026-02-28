// =============================================================
// GSR Sensor — Galvanic Skin Response
// =============================================================
#ifndef GSR_SENSOR_H
#define GSR_SENSOR_H

#include <Arduino.h>

void   gsrInit();
float  gsrReadResistance();          // Ohms
String gsrGetStressLevel(float r);   // "Low", "Moderate", "High"

#endif // GSR_SENSOR_H
