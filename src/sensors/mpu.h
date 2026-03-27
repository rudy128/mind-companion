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

// Get total acceleration (sqrt(x²+y²+z²))
float mpuGetAccelMagnitude(); 

// Get temperature from sensor
float mpuGetTemperature();

// Check if movement exceeds a given threshold
bool  mpuMovementDetected(float threshold);

// Get change in total acceleration since last update
float mpuGetMovementDelta();

// Get change in each axis (absolute difference)
float mpuGetDeltaX();
float mpuGetDeltaY();
float mpuGetDeltaZ();

// Count how many axes changed more than the threshold
int   mpuGetActiveAxes(float threshold);

#endif 
