// =============================================================
// MPU6050 sensor (Gyroscope)
// Function declarations for reading data and detecting movement
// =============================================================
#ifndef MPU_SENSOR_H
#define MPU_SENSOR_H

#include <Arduino.h>

// Initialize the MPU6050 sensor (I2C setup)
bool  mpuInit();     

// Update sensor readings (acceleration, gyro, temperature)
void  mpuUpdate();            

// Get acceleration values for each axis
float mpuGetAccelX();
float mpuGetAccelY();
float mpuGetAccelZ();
float mpuGetTemperature();

// Check if movement exceeds a given threshold
bool  mpuMovementDetected(float threshold);

// Per-axis absolute deltas (current vs previous reading)
float mpuGetDeltaX();
float mpuGetDeltaY();
float mpuGetDeltaZ();

// Count how many axes changed more than the threshold
int   mpuGetActiveAxes(float threshold);

#endif 
