// =============================================================
// TFT Display — ILI9341 Implementation
// =============================================================
#include "tft_display.h"
#include "../config.h"
#include "../actuators/audio_quotes.h"
#include "../network/logger.h"
#include <vector>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);

// ── SPI mutex — prevents Core 0 (speech) and Core 1 (loop)
//    from colliding on the software-SPI bus simultaneously.
//    All tftUpdate* functions must take this before any tft.* call.
static SemaphoreHandle_t _tftMutex = nullptr;

// Helper macros — keep call sites clean
#define TFT_LOCK()   xSemaphoreTake(_tftMutex, portMAX_DELAY)
#define TFT_UNLOCK() xSemaphoreGive(_tftMutex)

// Previous values for change detection
static char    prevTime[9]    = "";
static int     prevBPM        = -1;
static bool    prevFinger     = false;
static int     prevMX = -999, prevMY = -999, prevMZ = -999;
static String  prevSleep      = "";
static String  prevStress     = "";
static bool    prevEmergency  = false;

// ============ Layout Constants ============
// Y positions for each section
#define Y_TIME       5
#define Y_MPU_LABEL  20
#define Y_MPU_VAL    45
#define Y_HR_LABEL   70
#define Y_HR_VAL     75
#define Y_HR_STATUS  105
#define Y_SLEEP_LABEL 130
#define Y_SLEEP_VAL   150
#define Y_STRESS_LABEL 180
#define Y_STRESS_VAL   205
#define Y_STRESS_QUOTE 230
#define Y_EMERGENCY    255
#define Y_SPEECH       270
#define Y_IP           290

void tftInit() {
    // Create mutex once — must happen before any other core can call tftUpdate*
    if (_tftMutex == nullptr) {
        _tftMutex = xSemaphoreCreateMutex();
    }
    tft.begin();
    tft.setRotation(0);
    tft.fillScreen(ILI9341_BLACK);
    Serial.println("[TFT] Display initialized");
}

void tftShowBootScreen() {
    TFT_LOCK();
    tft.fillScreen(ILI9341_BLACK);

    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(4);
    tft.setCursor(35, 60);
    tft.println("M.I.N.D.");

    tft.setTextSize(2);
    tft.setCursor(40, 110);
    tft.println("COMPANION");

    tft.setTextSize(1);
    tft.setTextColor(ILI9341_GREEN);
    tft.setCursor(20, 150);
    tft.println("Heart: MAX30102");
    tft.setCursor(20, 165);
    tft.println("Motion: MPU6050");
    tft.setCursor(20, 180);
    tft.println("Stress: GSR Sensor");
    tft.setCursor(20, 195);
    tft.println("Clock:  DS3231 RTC");
    tft.setCursor(20, 210);
    tft.println("Mic:    INMP441");
    tft.setCursor(20, 225);
    tft.println("Camera: OV2640");

    tft.setTextColor(ILI9341_CYAN);
    tft.setCursor(30, 260);
    tft.println("Initializing systems...");
    TFT_UNLOCK();

    delay(2500);
}

void tftDrawDashboardLabels() {
    TFT_LOCK();
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextSize(2);

    tft.setTextColor(ILI9341_CYAN);
    tft.setCursor(10, Y_MPU_LABEL);
    tft.println("MPU:");

    tft.setTextColor(ILI9341_RED);
    tft.setCursor(10, Y_HR_LABEL);
    tft.println("Heart:");

    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(10, Y_SLEEP_LABEL);
    tft.println("Sleep:");

    tft.setTextColor(ILI9341_GREEN);
    tft.setCursor(10, Y_STRESS_LABEL);
    tft.println("Mind:");
    TFT_UNLOCK();

    // Default heart rate display (already mutex-guarded inside)
    tftUpdateHeartRate(0, false);
}

void tftUpdateTime(const char* timeStr) {
    if (strcmp(prevTime, timeStr) == 0) return;
    strncpy(prevTime, timeStr, sizeof(prevTime) - 1);

    TFT_LOCK();
    tft.fillRect(140, Y_TIME, 130, 20, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(140, Y_TIME);
    tft.print(timeStr);
    TFT_UNLOCK();
}

void tftUpdateHeartRate(int bpm, bool fingerPresent) {
    if (bpm == prevBPM && fingerPresent == prevFinger) return;
    prevBPM    = bpm;
    prevFinger = fingerPresent;

    TFT_LOCK();
    // BPM value
    tft.fillRect(80, Y_HR_VAL, 160, 25, ILI9341_BLACK);
    tft.setTextColor(ILI9341_RED);
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
        tft.setTextColor(ILI9341_GREEN);
        tft.print("Finger Detected");
    } else {
        tft.setTextColor(ILI9341_RED);
        tft.print("No Finger");
    }
    TFT_UNLOCK();
}

void tftUpdateMPU(int x, int y, int z) {
    if (x == prevMX && y == prevMY && z == prevMZ) return;
    prevMX = x; prevMY = y; prevMZ = z;

    TFT_LOCK();
    tft.fillRect(10, Y_MPU_VAL, 220, 18, ILI9341_BLACK);
    tft.setTextColor(ILI9341_CYAN);
    tft.setTextSize(1);
    tft.setCursor(10, Y_MPU_VAL);
    tft.printf("X:%d  Y:%d  Z:%d", x, y, z);
    TFT_UNLOCK();
}

void tftUpdateSleep(const String& quality) {
    if (quality == prevSleep) return;
    prevSleep = quality;

    TFT_LOCK();
    tft.fillRect(10, Y_SLEEP_VAL, 220, 25, ILI9341_BLACK);
    tft.setTextSize(2);

    if (quality == "Deep Sleep")      tft.setTextColor(ILI9341_BLUE);
    else if (quality == "Light Sleep") tft.setTextColor(ILI9341_MAGENTA);
    else if (quality == "Restless")    tft.setTextColor(ILI9341_ORANGE);
    else                               tft.setTextColor(ILI9341_WHITE);

    tft.setCursor(10, Y_SLEEP_VAL);
    tft.println(quality);
    TFT_UNLOCK();
}

void tftUpdateStress(const String& level, float gsrValue) {
    if (level == prevStress) return;
    prevStress = level;

    TFT_LOCK();
    // Clear the full stress value + quote area (60 px tall to cover any wrapped text)
    tft.fillRect(0, Y_STRESS_VAL, 240, 60, ILI9341_BLACK);

    // Explicitly handle the 'not worn' message returned by the GSR logic
    if (level == "Please wear finger straps") {
        tft.setTextColor(ILI9341_RED);
        tft.setTextSize(2);            // size 2 = 12px/char wide, 16px tall
        tft.setCursor(10, Y_STRESS_VAL);
        tft.println("Please wear");
        tft.setCursor(10, Y_STRESS_VAL + 22);
        tft.println("finger straps!");
    }
    else if (level == "High") {
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(10, Y_STRESS_VAL);
        tft.println("HIGH STRESS");
        // Show motivational quote
        tft.setTextSize(1);
        tft.setCursor(10, Y_STRESS_QUOTE);
        
        // Pick a random quote from the hashmap keys
        std::vector<std::string> quoteKeys;
        for (const auto& pair : AUDIO_QUOTE_MAP) {
            quoteKeys.push_back(pair.first);
        }
        
        if (quoteKeys.size() > 0) {
            int randomIndex = random(0, quoteKeys.size());
            const char* selectedQuote = quoteKeys[randomIndex].c_str();
            tft.println(selectedQuote);
            
            // Play the corresponding audio using the hashmap lookup
            // Release TFT lock before audio playback to avoid deadlock
            TFT_UNLOCK();
            
            // Use the new function that directly looks up the hashmap
            if (playQuoteByHashmap(selectedQuote)) {
                LOG_INFO("TFT", "Playing audio for quote: '%s'", selectedQuote);
            } else {
                LOG_WARN("TFT", "Failed to play audio for quote: '%s'", selectedQuote);
            }
            
            TFT_LOCK();
        }
    } else if (level == "Moderate") {
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_YELLOW);
        tft.setCursor(10, Y_STRESS_VAL);
        tft.println("MODERATE");
    } else {
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_GREEN);
        tft.setCursor(10, Y_STRESS_VAL);
        tft.println("LOW / Calm");
    }
    TFT_UNLOCK();
}

void tftUpdateIP(const String& ip) {
    TFT_LOCK();
    tft.fillRect(10, Y_IP, 230, 20, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, Y_IP);
    tft.print("IP:" + ip);
    TFT_UNLOCK();
}

void tftUpdateEmergency(bool active) {
    if (active == prevEmergency) return;
    prevEmergency = active;

    TFT_LOCK();
    tft.fillRect(0, Y_EMERGENCY, 240, 15, ILI9341_BLACK);
    if (active) {
        tft.setTextColor(ILI9341_RED);
        tft.setTextSize(2);
        tft.setCursor(10, Y_EMERGENCY);
        tft.print("!! EMERGENCY !!");
    }
    TFT_UNLOCK();
}

void tftUpdateSpeechStatus(const String& text) {
    TFT_LOCK();
    tft.fillRect(0, Y_SPEECH, 240, 15, ILI9341_BLACK);
    if (text.length() > 0) {
        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(1);
        tft.setCursor(10, Y_SPEECH);
        tft.print("Voice: " + text.substring(0, 30));
    }
    TFT_UNLOCK();
}

void tftShowListening(bool active) {
    TFT_LOCK();
    tft.fillRect(0, Y_SPEECH, 240, 15, ILI9341_BLACK);
    if (active) {
        tft.setTextColor(ILI9341_CYAN);
        tft.setTextSize(1);
        tft.setCursor(10, Y_SPEECH);
        tft.print("Listening...");
    }
    TFT_UNLOCK();
}

Adafruit_ILI9341& tftGetDisplay() {
    return tft;
}
