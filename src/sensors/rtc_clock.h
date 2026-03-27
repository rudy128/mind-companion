// =============================================================
// DS3231 RTC (Real-Time Clock)
// Function declarations for time reading and formatting
// =============================================================
#ifndef RTC_CLOCK_H
#define RTC_CLOCK_H

#include <Arduino.h>
#include <RTClib.h>

// Initialize RTC (I2C communication setup)
bool     rtcInit();               

// Get full date and time from RTC
DateTime rtcGetNow(); 

// Format time as "HH:MM:SS"
void     rtcFormatTime(char* buf, size_t len); 

// Get individual time values
uint8_t  rtcGetHour();
uint8_t  rtcGetMinute();
uint8_t  rtcGetSecond();

// Set RTC to compile time (run once during setup)
void     rtcSetToCompileTime();

#endif 
