// =============================================================
// RTC DS3231 Implementation
// =============================================================
#include "rtc_clock.h"
#include "../config.h"

static RTC_DS3231 rtc;

// Canada-style DST:
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

    // RTClib uses dayOfTheWeek(): 0 = Sunday ... 6 = Saturday
    auto dow = [](int y, int m, int d) -> int {
        return DateTime(y, m, d, 0, 0, 0).dayOfTheWeek();
    };

    auto secondSundayInMarch = [&]() -> int {
        int dowFirst = dow(year, 3, 1);                 // 0..6
        int firstSunday = 1 + ((7 - dowFirst) % 7);  // 1..7
        return firstSunday + 7;                        // second Sunday
    };

    auto firstSundayInNovember = [&]() -> int {
        int dowFirst = dow(year, 11, 1);
        int firstSunday = 1 + ((7 - dowFirst) % 7);
        return firstSunday; // first Sunday in November
    };

    const int startDay = secondSundayInMarch();
    const int endDay   = firstSundayInNovember();

    // Months outside [3..11] cannot be DST.
    if (month < 3 || month > 11) return false;
    if (month > 3 && month < 11) return true;

    // March: DST starts at 02:00 on the second Sunday.
    if (month == 3) {
        if (day < startDay) return false;
        if (day > startDay) return true;
        return hour >= 2;
    }

    // November: DST ends at 02:00 on the first Sunday (DST active before 02:00).
    if (month == 11) {
        if (day < endDay) return true;
        if (day > endDay) return false;
        return hour < 2;
    }

    return false;
#endif
}

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
    // If you keep the DS3231 on "standard time", apply DST for display.
    // Assumption: DST offset is only +1 hour (Canada-style rules).
    const int offsetHours = rtcIsDstCanada(now) ? 1 : 0;
    // TimeSpan(days, hours, minutes, seconds)
    DateTime adj = now + TimeSpan(0, (int8_t)offsetHours, 0, 0);
    snprintf(buf, len, "%02d:%02d:%02d", adj.hour(), adj.minute(), adj.second());
}

uint8_t rtcGetHour()   { return rtc.now().hour(); }
uint8_t rtcGetMinute() { return rtc.now().minute(); }
uint8_t rtcGetSecond() { return rtc.now().second(); }

void rtcSetToCompileTime() {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("[RTC] Time set to compile time");
}
