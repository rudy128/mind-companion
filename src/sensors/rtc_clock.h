// =============================================================
// DS3231 RTC (Real-Time Clock)
// Function declarations for time reading and formatting
// =============================================================
#ifndef RTC_CLOCK_H
#define RTC_CLOCK_H

#include <Arduino.h>

bool     rtcInit();               // initialize RTC on I2C bus
void     rtcFormatTime(char* buf, size_t len); // "HH:MM:SS"
uint8_t  rtcGetSecond();

#endif // RTC_CLOCK_H
