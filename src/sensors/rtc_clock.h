// =============================================================
// RTC DS3231 — Real-Time Clock
// =============================================================
#ifndef RTC_CLOCK_H
#define RTC_CLOCK_H

#include <Arduino.h>
#include <RTClib.h>

bool     rtcInit();               // initialize RTC on I2C bus
DateTime rtcGetNow();             // current date/time
void     rtcFormatTime(char* buf, size_t len); // "HH:MM:SS"
uint8_t  rtcGetHour();
uint8_t  rtcGetMinute();
uint8_t  rtcGetSecond();

// Call this once (first flash) to set RTC to compile time
void     rtcSetToCompileTime();

#endif // RTC_CLOCK_H
