// =============================================================
// M.I.N.D. Companion — Central Configuration
// All pin definitions, credentials, timing constants
// =============================================================
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========================= WiFi =========================
#define WIFI_SSID          "Chanpreet"
#define WIFI_PASSWORD      "chanpreet09"
#define WIFI_RETRY_MAX     30        // max connection attempts

// ========================= OpenAI API =========================
// !! In production, move this to a secrets file that is .gitignored !!
#define OPENAI_API_KEY     "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"

// ========================= MQTT Broker =========================
// Local Mosquitto broker running on your laptop.
// Replace MQTT_SERVER with your laptop's LAN IP before flashing.
#define MQTT_SERVER        "172.20.10.3"   // ← your laptop LAN IP
#define MQTT_PORT          1883
#define MQTT_CLIENT_ID     "mind-companion"
// Topics — publish
#define MQTT_TOPIC_DATA    "mind/data"       // sensor telemetry JSON (1 Hz)
#define MQTT_TOPIC_LOGS    "mind/logs"       // log messages
#define MQTT_TOPIC_ALERT   "mind/alert"      // emergency events
// Topics — subscribe
#define MQTT_TOPIC_CMD     "mind/cmd"        // commands from dashboard

// ========================= I2C Bus =========================
#define I2C_SDA            47
#define I2C_SCL            48
#define MPU_ADDRESS        0x69     // MPU6050 AD0 pulled high

// ========================= TFT Display (ILI9341, Software SPI) =========================
#define TFT_CS             39
#define TFT_RST            40
#define TFT_DC             42
#define TFT_MOSI           2
#define TFT_SCK            1

// ========================= MAX30102/30105 Heart Rate =========================
// Uses shared I2C bus (GPIO47/48) — default I2C address 0x57

// ========================= GSR Sensor =========================
#define GSR_PIN            3        // ADC input
#define GSR_ADC_MAX        4095.0f
#define GSR_R_REF          10000.0f // reference resistor in voltage divider
#define GSR_SAMPLES        10       // number of ADC samples to average

// ========================= Emergency Button =========================
#define BUTTON_PIN              41       // active LOW with internal pull-up
#define DOUBLE_PRESS_WINDOW_MS  400      // max time between presses for double-press
#define LONG_PRESS_MS           1500     // hold time for long press (future use)
#define DEBOUNCE_MS             50       // button debounce time

// ========================= LED Strip (Breathing Pattern) =========================
#define LED_PIN            20       // PWM output via PNP transistor
#define LED_PWM_FREQ       5000
#define LED_PWM_RESOLUTION 8        // 8-bit → 0-255

// ========================= Vibration Motor =========================
#define VIBRATION_PIN      46

// ========================= Microphone (INMP441 via I2S #0 RX) =========================
#define MIC_I2S_PORT       I2S_NUM_0
#define MIC_I2S_SCK        45
#define MIC_I2S_WS         38
#define MIC_I2S_SD         19
#define MIC_SAMPLE_RATE    16000
#define MIC_RECORD_SECONDS 2

// ========================= Speaker (MAX98357A via I2S #1 TX) =========================
#define SPK_I2S_PORT       I2S_NUM_1
#define SPK_I2S_BCLK       21
#define SPK_I2S_LRC        0
#define SPK_I2S_DIN        14
#define SPK_SAMPLE_RATE    22050
#define SPK_SD_PIN         -1 

// ========================= Camera (ESP32-S3 built-in OV2640) =========================
#define CAM_PIN_PWDN       -1
#define CAM_PIN_RESET      -1
#define CAM_PIN_XCLK       15
#define CAM_PIN_SIOD       4
#define CAM_PIN_SIOC       5
#define CAM_PIN_D0         11
#define CAM_PIN_D1         9
#define CAM_PIN_D2         8
#define CAM_PIN_D3         10
#define CAM_PIN_D4         12
#define CAM_PIN_D5         18
#define CAM_PIN_D6         17
#define CAM_PIN_D7         16
#define CAM_PIN_VSYNC      6
#define CAM_PIN_HREF       7
#define CAM_PIN_PCLK       13

// ========================= Timing Intervals (ms) =========================
#define INTERVAL_GSR_CHECK       1000UL       // GSR stress reading — 1s
#define INTERVAL_MPU_DISPLAY     1000UL       // MPU accel on TFT — 1s
#define INTERVAL_SPEECH_RECOG    15000UL      // speech recognition cycle
#define INTERVAL_VIBRATION_HOUR  60000UL      // 1 minute vibration reminder
#define INTERVAL_AWAKE_NUDGE     60000UL      // vibrate + open camera every 1 min while awake
#define INTERVAL_SLEEP_WINDOW    30000UL      // sleep quality evaluation window
#define INTERVAL_DASHBOARD_POLL  2000UL       // web dashboard AJAX poll (JS-side uses 1000ms now)
#define INTERVAL_DISPLAY_UPDATE  1000UL       // RTC time on TFT

// ========================= Thresholds =========================
#define MOVEMENT_THRESHOLD       0.2f   // g-force delta for "movement detected"
#define SLEEP_MOVEMENT_DEEP      0.3f   // below this = deep sleep
#define SLEEP_MOVEMENT_LIGHT     0.8f   // below this = light sleep; above = restless
#define HR_ABNORMAL_HIGH         120    // BPM above this triggers breathing LED
#define HR_ABNORMAL_LOW          50     // BPM below this triggers breathing LED
#define NO_MOVEMENT_EMERGENCY_MS 40000  // 40 seconds → speaker alarm
//#define GSR_HIGH_THRESHOLD       24000.0f
//#define GSR_MODERATE_LOW         14000.0f
//#define GSR_LOW_THRESHOLD         7900.0f
#define GSR_WEAR_THRESHOLD      8000.0f
#define GSR_LOW_THRESHOLD       4000.0f
#define GSR_MODERATE_THRESHOLD  2000.0f

// ========================= Audio Quotes System =========================
// Audio quotes are now managed by the audio_quotes module.
// Audio files must be in LittleFS as MP3 files.
// The data/ folder becomes / in LittleFS storage.

#include <map>
#include <string>

// Simple quote-to-audio mapping
// Key: Quote text, Value: Audio file path in LittleFS
static std::map<std::string, std::string> AUDIO_QUOTE_MAP = {
    // CALM quotes
    {"Take a deep breath", "/q1.mp3"},
    {"This feeling will pass", "/q1.mp3"},
    {"Slow down, you got this", "/q1.mp3"},
    
    // MOTIVATE quotes
    {"You are stronger than you think", "/q1.mp3"},
    {"One step at a time", "/q1.mp3"},
    {"It's okay to ask for help", "/q1.mp3"}
};

#endif // CONFIG_H
