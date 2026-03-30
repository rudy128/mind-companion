// ==========================================================================================
// This file contains the implementation for the TFT Display using ILI9341 library
// It includes functions to initialize the display, show a boot screen, and show update
// for various sections (time, heart rate, sleep, stress, emergency, listening status).
// ==========================================================================================
#include "tft_display.h"
#include <Adafruit_ILI9341.h>
#include "../config.h"
#include "../actuators/audio_quotes.h"
#include "../network/logger.h"
#include <vector>
#include <string>
#include <cstdio>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);

// ── SPI Lock— prevents Core 0 (speech) and Core 1 (loop)
//    from colliding on the software-SPI bus simultaneously.
//    All tftUpdate* functions must take this before any tft.* call.
static SemaphoreHandle_t _tftMutex = nullptr;

// Lock/Unlock macros — keep call sites clean
#define TFT_LOCK()   xSemaphoreTake(_tftMutex, portMAX_DELAY)
#define TFT_UNLOCK() xSemaphoreGive(_tftMutex)

// Light palette (RGB565) — all text on black BG; no dark blue, no saturated red
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
static const uint16_t L_CLOUD   = rgb565(235, 238, 245);  // soft white
static const uint16_t L_ROSE    = rgb565(255, 175, 185); // light coral / was red
static const uint16_t L_MINT    = rgb565(165, 245, 205); // light mint / was green
static const uint16_t L_LEMON   = rgb565(255, 245, 190); // light yellow
static const uint16_t L_TITLE_YELLOW = rgb565(255, 235, 145); // M.I.N.D. title — always this on main + boot
static const uint16_t L_AQUA    = rgb565(150, 235, 255); // light cyan
static const uint16_t L_SKY     = rgb565(165, 215, 255); // Deep Sleep — pale sky (not dark blue)
static const uint16_t L_LILAC   = rgb565(225, 195, 255); // Light Sleep
static const uint16_t L_APRICOT = rgb565(255, 205, 175); // Restless

// Adafruit GFX has no usable ° glyph — draw a small ring, then "C".
// Do not use getCursorX() after printf on ESP32: the cursor often stops mid-string (e.g. at
// '.'), which drew the ring on the decimal. Use getTextBounds on the same string instead.
static void tftDrawDegreeC(uint16_t color, int16_t textStartX, int16_t baselineY, const char* numText) {
    int16_t         bx1, by1;
    uint16_t        tw, th;
    tft.getTextBounds(numText, textStartX, baselineY, &bx1, &by1, &tw, &th);
    const int16_t xAfter = bx1 + (int16_t)tw;
    const int16_t gap    = 2;
    const int16_t cx     = xAfter + gap + 2;
    const int16_t cy     = baselineY - 6;
    tft.drawCircle(cx, cy, 2, color);
    tft.setCursor(xAfter + gap + 10, baselineY);
    tft.print("C");
}

// Previous values for change detection so we do not reprint it
static char    prevTime[9]    = "";
static int     prevBPM        = -1;
static bool    prevFinger     = false;
static String  prevSleep      = "";
static String  prevStress     = "";
static bool    prevEmergency  = false;
static int     prevTempKey    = -99999;  // sensorOk ? tenths °C : -1

// ============ Layout Positions ============
// Top → bottom: title, Heart, Temp, Stress, Sleep, Emergency, Voice
#define Y_TITLE        4
#define Y_TIME         4
#define Y_HR_LABEL     28
#define Y_HR_VAL       33
#define Y_HR_STATUS    63
#define Y_TEMP_LABEL   86
#define Y_TEMP_VAL     91
#define Y_STRESS_LABEL 114
#define Y_STRESS_VAL   138
#define Y_STRESS_QUOTE 163
#define Y_SLEEP_LABEL  205
#define Y_SLEEP_VAL    225
#define Y_EMERGENCY    248
#define Y_VOICE_LABEL  268
#define Y_VOICE_TEXT   284

//Initialize TFT
void tftInit() {
    // Create mutex once 
    if (_tftMutex == nullptr) {
        _tftMutex = xSemaphoreCreateMutex();
    }
    tft.begin();
    tft.setRotation(0);
    tft.fillScreen(ILI9341_BLACK);
    Serial.println("[TFT] Display initialized");
}

// Boot splash — M.I.N.D. COMPANION + IP; light palette (yellow title, cyan/mint body)
void tftShowBootScreen(const String& ipLine) {
    TFT_LOCK();
    tft.fillScreen(ILI9341_BLACK);

    tft.setTextColor(L_TITLE_YELLOW);
    tft.setTextSize(3);
    tft.setCursor(10, 48);
    tft.println("M.I.N.D.");
    tft.setCursor(10, 82);
    tft.println("COMPANION");

    tft.setTextSize(2);
    tft.setTextColor(L_AQUA);
    tft.setCursor(10, 128);
    tft.print("IP ");
    tft.println(ipLine);

    tft.setTextSize(1);
    tft.setTextColor(L_MINT);
    tft.setCursor(16, 162);
    tft.println("Heart: MAX30102");
    tft.setTextColor(L_AQUA);
    tft.setCursor(16, 178);
    tft.println("Stress: GSR Sensor");
    tft.setTextColor(L_MINT);
    tft.setCursor(16, 194);
    tft.println("Clock:  DS3231 RTC");
    tft.setTextColor(L_AQUA);
    tft.setCursor(16, 210);
    tft.println("Mic:    INMP441");
    tft.setTextColor(L_MINT);
    tft.setCursor(16, 226);
    tft.println("Camera: OV2640");

    tft.setTextColor(L_AQUA);
    tft.setCursor(24, 258);
    tft.println("Starting...");
    TFT_UNLOCK();

    delay(2500);
}

//Draw main dashboard — title + section labels (Heart, Temp, Stress, Sleep, Voice)
void tftDrawDashboardLabels() {
    TFT_LOCK();
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(L_TITLE_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(10, Y_TITLE);
    tft.print("M.I.N.D. Companion");

    tft.setTextSize(2);
    tft.setTextColor(L_ROSE);
    tft.setCursor(10, Y_HR_LABEL);
    tft.println("Heart:");

    tft.setTextColor(L_AQUA);
    tft.setCursor(10, Y_TEMP_LABEL);
    tft.println("Temp:");

    tft.setTextColor(L_MINT);
    tft.setCursor(10, Y_STRESS_LABEL);
    tft.println("Stress:");

    tft.setTextColor(L_LEMON);
    tft.setCursor(10, Y_SLEEP_LABEL);
    tft.println("Sleep:");

    tft.setTextColor(L_CLOUD);
    tft.setCursor(10, Y_VOICE_LABEL);
    tft.println("Voice:");
    TFT_UNLOCK();

    tftUpdateHeartRate(0, false);
    tftUpdateTemperature(false, 0.f);
}

//Update time 
void tftUpdateTime(const char* timeStr) {
    if (strcmp(prevTime, timeStr) == 0) return;
    strncpy(prevTime, timeStr, sizeof(prevTime) - 1);

    TFT_LOCK();
    tft.fillRect(140, Y_TIME, 130, 20, ILI9341_BLACK);
    tft.setTextColor(L_CLOUD);
    tft.setTextSize(2);
    tft.setCursor(140, Y_TIME);
    tft.print(timeStr);
    TFT_UNLOCK();
}

//Update heart rate section with BPM and finger status
void tftUpdateHeartRate(int bpm, bool fingerPresent) {
    if (bpm == prevBPM && fingerPresent == prevFinger) return;
    prevBPM    = bpm;
    prevFinger = fingerPresent;

    TFT_LOCK();
    // BPM value
    tft.fillRect(80, Y_HR_VAL, 160, 25, ILI9341_BLACK);
    tft.setTextColor(L_ROSE);
    tft.setTextSize(2);
    tft.setCursor(80, Y_HR_VAL);
    if (fingerPresent && bpm > 0) {
        tft.printf("%d bpm", bpm);
    } else {
        tft.print("-- bpm");
    }

    // Finger status
    tft.fillRect(80, Y_HR_STATUS, 160, 20, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setCursor(80, Y_HR_STATUS + 5);
    if (fingerPresent) {
        tft.setTextColor(L_MINT);
        tft.print("Measuring");
    } else {
        tft.setTextColor(L_ROSE);
        tft.print("No Finger");
    }
    TFT_UNLOCK();
}

// Temperature — one decimal + drawn degree ring + C (see tftDrawDegreeC)
void tftUpdateTemperature(bool sensorOk, float celsius) {
    int key = -1;
    if (sensorOk) {
        float t = celsius * 10.f;
        key = (int)(t + (t >= 0.f ? 0.5f : -0.5f));
    }
    if (key == prevTempKey) return;
    prevTempKey = key;

    TFT_LOCK();
    tft.fillRect(80, Y_TEMP_VAL, 160, 22, ILI9341_BLACK);
    tft.setTextColor(L_AQUA);
    tft.setTextSize(2);
    tft.setCursor(80, Y_TEMP_VAL);
    char tempBuf[16];
    if (sensorOk) {
        snprintf(tempBuf, sizeof(tempBuf), "%.1f", celsius);
    } else {
        snprintf(tempBuf, sizeof(tempBuf), "--");
    }
    tft.print(tempBuf);
    tftDrawDegreeC(L_AQUA, 80, Y_TEMP_VAL, tempBuf);
    TFT_UNLOCK();
}

//Update sleep quality status
void tftUpdateSleep(const String& quality) {
    if (quality == prevSleep) return;
    prevSleep = quality;

    TFT_LOCK();
    tft.fillRect(10, Y_SLEEP_VAL, 220, 25, ILI9341_BLACK);
    tft.setTextSize(2);

    if (quality == "Deep Sleep")       tft.setTextColor(L_SKY);
    else if (quality == "Light Sleep") tft.setTextColor(L_LILAC);
    else if (quality == "Restless")    tft.setTextColor(L_APRICOT);
    else                               tft.setTextColor(L_CLOUD);

    tft.setCursor(10, Y_SLEEP_VAL);
    tft.println(quality);
    TFT_UNLOCK();
}

//Update stress level and show motivational quote if high
// Caller must hold TFT_LOCK. Clears quote line, picks random AUDIO_QUOTE_MAP key, draws it, plays matching audio.
// Returns with TFT_LOCK held (unlocks during playback).
static void tftDrawHighStressRandomQuoteAndPlayLocked() {
    std::vector<std::string> quoteKeys;
    for (const auto& pair : AUDIO_QUOTE_MAP) {
        quoteKeys.push_back(pair.first);
    }
    if (quoteKeys.empty()) return;

    int randomIndex = random(0, (int)quoteKeys.size());
    const std::string& keyStr = quoteKeys[randomIndex];
    const char* selectedQuote = keyStr.c_str();

    tft.fillRect(0, Y_STRESS_QUOTE, 240, 40, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(L_ROSE);
    tft.setCursor(10, Y_STRESS_QUOTE);
    tft.println(selectedQuote);

    TFT_UNLOCK();
    if (playQuoteByHashmap(selectedQuote)) {
        LOG_INFO("TFT", "Playing audio for quote: '%s'", selectedQuote);
    } else {
        LOG_WARN("TFT", "Failed to play audio for quote: '%s'", selectedQuote);
    }
    TFT_LOCK();
}

void tftHighStressRefreshRandomQuote() {
    TFT_LOCK();
    if (prevStress != "High") {
        TFT_UNLOCK();
        return;
    }
    tftDrawHighStressRandomQuoteAndPlayLocked();
    TFT_UNLOCK();
}

void tftUpdateStress(const String& level) {
    if (level == prevStress) return;
    prevStress = level;

    TFT_LOCK();
    // Clear the full stress value + quote area (60 px tall to cover any wrapped text)
    tft.fillRect(0, Y_STRESS_VAL, 240, 60, ILI9341_BLACK);

    // Handle the 'not worn' message returned by the GSR logic
    if (level == "Please wear finger straps") {
        tft.setTextColor(L_ROSE);
        tft.setTextSize(2);            // size 2 = 12px/char wide, 16px tall
        tft.setCursor(10, Y_STRESS_VAL);
        tft.println("Please wear");
        tft.setCursor(10, Y_STRESS_VAL + 22);
        tft.println("finger straps!");
    }
    else if (level == "High") {
        tft.setTextSize(2);
        tft.setTextColor(L_ROSE);
        tft.setCursor(10, Y_STRESS_VAL);
        tft.println("HIGH STRESS");
        tftDrawHighStressRandomQuoteAndPlayLocked();
    } else if (level == "Moderate") {
        tft.setTextSize(2);
        tft.setTextColor(L_LEMON);
        tft.setCursor(10, Y_STRESS_VAL);
        tft.println("MODERATE");
    } else {
        tft.setTextSize(2);
        tft.setTextColor(L_MINT);
        tft.setCursor(10, Y_STRESS_VAL);
        tft.println("LOW / Calm");
    }
    TFT_UNLOCK();
}

//Updates emergency status on TFT when alert is active
void tftUpdateEmergency(bool active) {
    if (active == prevEmergency) return;
    prevEmergency = active;

    TFT_LOCK();
    tft.fillRect(0, Y_EMERGENCY, 240, 15, ILI9341_BLACK);
    if (active) {
        tft.setTextColor(L_ROSE);
        tft.setTextSize(2);
        tft.setCursor(10, Y_EMERGENCY);
        tft.print("!! EMERGENCY !!");
    }
    TFT_UNLOCK();
}

// Speech line (label "Voice:" is static in tftDrawDashboardLabels)
void tftUpdateSpeechStatus(const String& text) {
    TFT_LOCK();
    tft.fillRect(0, Y_VOICE_TEXT, 240, 16, ILI9341_BLACK);
    if (text.length() > 0) {
        tft.setTextColor(L_CLOUD);
        tft.setTextSize(1);
        tft.setCursor(10, Y_VOICE_TEXT);
        tft.print(text.substring(0, 36));
    }
    TFT_UNLOCK();
}

void tftShowListening(bool active) {
    TFT_LOCK();
    tft.fillRect(0, Y_VOICE_TEXT, 240, 16, ILI9341_BLACK);
    if (active) {
        tft.setTextColor(L_AQUA);
        tft.setTextSize(1);
        tft.setCursor(10, Y_VOICE_TEXT);
        tft.print("Listening...");
    }
    TFT_UNLOCK();
}
