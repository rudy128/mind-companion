// =====================================================================
// DS3231 RTC (Real-Time Clock)
// Handles time reading, formatting, and DST(DAY LIGHT TIME) adjustment
// =====================================================================
#include "rtc_clock.h"
#include "../config.h"
#include <RTClib.h>

static RTC_DS3231 rtc;

// Check if current time falls under Canada DST rules
// - start: second Sunday in March at 02:00
// - end  : first Sunday in November at 02:00
static bool rtcIsDstCanada(const DateTime& now) {
#if !defined(RTC_APPLY_CANADA_DST)
    (void)now;
    return false;
#else
    const int year  = now.year();
    const int month = now.month();
    const int day   = now.day();
    const int hour  = now.hour();

     // Get day of week (0 = Sunday, 6 = Saturday)
    auto dow = [](int y, int m, int d) -> int {
        return DateTime(y, m, d, 0, 0, 0).dayOfTheWeek();
    };


    // Find second Sunday in March
    auto secondSundayInMarch = [&]() -> int {
        int dowFirst = dow(year, 3, 1);                 // 0..6
        int firstSunday = 1 + ((7 - dowFirst) % 7);  // 1..7
        return firstSunday + 7;                        // second Sunday
    };

    // Find first Sunday in November
    auto firstSundayInNovember = [&]() -> int {
        int dowFirst = dow(year, 11, 1);
        int firstSunday = 1 + ((7 - dowFirst) % 7);
        return firstSunday; // first Sunday in November
    };

    const int startDay = secondSundayInMarch();
    const int endDay   = firstSundayInNovember();

    // Outside March–November → no DST
    if (month < 3 || month > 11) return false;

    // Between April–October → always DST
    if (month > 3 && month < 11) return true;

    // March: check if DST has started
    if (month == 3) {
        if (day < startDay) return false;
        if (day > startDay) return true;
        return hour >= 2;
    }

    // November: check if DST has ended
    if (month == 11) {
        if (day < endDay) return true;
        if (day > endDay) return false;
        return hour < 2;
    }

    return false;
#endif
}

// Initialize RTC module
bool rtcInit() {
    Serial.println("[RTC] Initializing DS3231...");
    // Check if RTC is connected
    if (!rtc.begin()) {
        Serial.println("[RTC] ERROR: DS3231 not found!");
        return false;
    }
    Serial.println("[RTC] DS3231 OK");
    return true;
}

void rtcFormatTime(char* buf, size_t len) {
    DateTime now = rtc.now();
    
       // Add +1 hour if DST is active
    const int offsetHours = rtcIsDstCanada(now) ? 1 : 0;

    // TimeSpan(days, hours, minutes, seconds)
    DateTime adj = now + TimeSpan(0, (int8_t)offsetHours, 0, 0);
    snprintf(buf, len, "%02d:%02d:%02d", adj.hour(), adj.minute(), adj.second());
}

uint8_t rtcGetSecond() { return rtc.now().second(); }
