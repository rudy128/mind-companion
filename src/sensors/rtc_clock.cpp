// =============================================================
// RTC DS3231 Implementation
// =============================================================
#include "rtc_clock.h"
#include "../config.h"

static RTC_DS3231 rtc;

bool rtcInit() {
    Serial.println("[RTC] Initializing DS3231...");
    if (!rtc.begin()) {
        Serial.println("[RTC] ERROR: DS3231 not found!");
        return false;
    }
    // Uncomment the next line ONCE to set RTC to compile time, then re-comment
    // rtcSetToCompileTime();
    Serial.println("[RTC] DS3231 OK");
    return true;
}

DateTime rtcGetNow() {
    return rtc.now();
}

void rtcFormatTime(char* buf, size_t len) {
    DateTime now = rtc.now();
    snprintf(buf, len, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
}

uint8_t rtcGetHour()   { return rtc.now().hour(); }
uint8_t rtcGetMinute() { return rtc.now().minute(); }
uint8_t rtcGetSecond() { return rtc.now().second(); }

void rtcSetToCompileTime() {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("[RTC] Time set to compile time");
}
