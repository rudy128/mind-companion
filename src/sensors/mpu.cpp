// =============================================================
// MPU6050 — Accelerometer / Gyroscope Implementation
// =============================================================
#include "mpu.h"
#include "../config.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

static Adafruit_MPU6050 mpu;

static float ax = 0, ay = 0, az = 0;
static float gx = 0, gy = 0, gz = 0;
static float temp = 0;
static float lastAx = 0, lastAy = 0, lastAz = 0;
static float lastMag = 0;

static float mag(float x, float y, float z) {
    return sqrtf(x * x + y * y + z * z);
}

bool mpuInit() {
    Serial.println("[MPU] Initializing MPU6050...");
    if (!mpu.begin(MPU_ADDRESS)) {
        Serial.println("[MPU] ERROR: MPU6050 not found!");
        return false;
    }
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    lastMag = 9.81f; // approximate 1G at rest
    Serial.println("[MPU] MPU6050 OK");
    return true;
}

void mpuUpdate() {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);

    // save previous
    lastAx = ax; lastAy = ay; lastAz = az;
    lastMag = mag(ax, ay, az);

    ax = a.acceleration.x;
    ay = a.acceleration.y;
    az = a.acceleration.z;
    gx = g.gyro.x;
    gy = g.gyro.y;
    gz = g.gyro.z;
    temp = t.temperature;
}

float mpuGetAccelX()         { return ax; }
float mpuGetAccelY()         { return ay; }
float mpuGetAccelZ()         { return az; }
float mpuGetTemperature()    { return temp; }

float mpuGetAccelMagnitude() { return mag(ax, ay, az); }

float mpuGetMovementDelta() {
    float curMag = mag(ax, ay, az);
    return fabsf(curMag - lastMag);
}

bool mpuMovementDetected(float threshold) {
    float dx = fabsf(ax - lastAx);
    float dy = fabsf(ay - lastAy);
    float dz = fabsf(az - lastAz);
    return (dx > threshold || dy > threshold || dz > threshold);
}
