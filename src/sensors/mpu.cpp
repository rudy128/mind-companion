// =============================================================
/// MPU6050 sensor (Gyroscope)
// Handles initialization, reading data, and movement detection
// =============================================================
#include "mpu.h"
#include "../config.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Create MPU object
static Adafruit_MPU6050 mpu;

// Current sensor values
static float ax = 0, ay = 0, az = 0;    // acceleration (m/s²)
static float gx = 0, gy = 0, gz = 0;   //gyro (rad/s)
static float temp = 0;           // temperature (°C)

// Previous readings (used to detect movement)
static float lastAx = 0, lastAy = 0, lastAz = 0;
static float lastMag = 0;

// Calculate magnitude of acceleration vector
static float mag(float x, float y, float z) {
    return sqrtf(x * x + y * y + z * z);
}

// Initialize MPU6050 sensor
bool mpuInit() {
    Serial.println("[MPU] Initializing MPU6050...");
     // Check if sensor is connected
    if (!mpu.begin(MPU_ADDRESS)) {
        Serial.println("[MPU] ERROR: MPU6050 not found!");
        return false;
    }

     // Set sensor ranges
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    // Assume device is at rest initially (~1G)
    lastMag = 9.81f; 
    Serial.println("[MPU] MPU6050 OK");
    return true;
}

// Read new data from sensor
void mpuUpdate() {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);

    // Store previous values before updating
    lastAx = ax; lastAy = ay; lastAz = az;
    lastMag = mag(ax, ay, az);

    // Update with new readings
    ax = a.acceleration.x;
    ay = a.acceleration.y;
    az = a.acceleration.z;
    gx = g.gyro.x;
    gy = g.gyro.y;
    gz = g.gyro.z;
    temp = t.temperature;
}

// Getter functions for acceleration
float mpuGetAccelX()         { return ax; }
float mpuGetAccelY()         { return ay; }
float mpuGetAccelZ()         { return az; }

// Get current temperature
float mpuGetTemperature()    { return temp; }

// Get total acceleration magnitude
float mpuGetAccelMagnitude() { return mag(ax, ay, az); }

// Difference between current and previous magnitude
float mpuGetMovementDelta() {
    float curMag = mag(ax, ay, az);
    return fabsf(curMag - lastMag);
}

// Check if movement exceeds threshold on any axis
bool mpuMovementDetected(float threshold) {
    float dx = fabsf(ax - lastAx);
    float dy = fabsf(ay - lastAy);
    float dz = fabsf(az - lastAz);
    return (dx > threshold || dy > threshold || dz > threshold);
}

// Change in each axis (absolute difference) 
float mpuGetDeltaX() { return fabsf(ax - lastAx); }
float mpuGetDeltaY() { return fabsf(ay - lastAy); }
float mpuGetDeltaZ() { return fabsf(az - lastAz); }

// Count how many axes changed more than threshold
int mpuGetActiveAxes(float threshold) {
    int count = 0;
    if (fabsf(ax - lastAx) > threshold) count++;
    if (fabsf(ay - lastAy) > threshold) count++;
    if (fabsf(az - lastAz) > threshold) count++;
    return count;
}
