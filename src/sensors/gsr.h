// =============================================================
// GSR Sensor — Galvanic Skin Response
// =============================================================
#ifndef GSR_SENSOR_H
#define GSR_SENSOR_H

#include <Arduino.h>

void   gsrInit();
float  gsrReadConductance();         // µS (microsiemens)
String gsrGetStressLevel(float c);   // "Low", "Moderate", "High"

#endif // GSR_SENSOR_H
