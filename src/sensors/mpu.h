// =============================================================
// MPU6050 — Accelerometer / Gyroscope
// =============================================================
#ifndef MPU_SENSOR_H
#define MPU_SENSOR_H

#include <Arduino.h>

bool  mpuInit();              // initialize MPU6050 on I2C
void  mpuUpdate();            // read latest accel/gyro/temp

float mpuGetAccelX();
float mpuGetAccelY();
float mpuGetAccelZ();
float mpuGetAccelMagnitude(); // sqrt(x²+y²+z²)
float mpuGetTemperature();

// Movement detection: compares current vs previous reading
bool  mpuMovementDetected(float threshold);

// Returns the delta between current and last magnitude
float mpuGetMovementDelta();

// Per-axis absolute deltas (current vs previous reading)
float mpuGetDeltaX();
float mpuGetDeltaY();
float mpuGetDeltaZ();

// Count of axes that changed by more than `threshold` m/s²
int   mpuGetActiveAxes(float threshold);

#endif // MPU_SENSOR_H
