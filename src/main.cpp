// =============================================================
//  M.I.N.D. Companion — Main Orchestrator
//  ESP32-S3 WROOM (Freenove) • Arduino Framework • PlatformIO
// =============================================================
//
//  Core split:
//    Core 1 (loop) — sensors, TFT display, actuators, web server
//                    Runs every ~1 ms with no blocking waits.
//    Core 0 (task) — speech recording + OpenAI HTTP (can take 2-5s)
//                    Isolated so it never stalls Core 1.
//
// =============================================================

#include <Arduino.h>
#include <Wire.h>
#include <esp_task_wdt.h>   // watchdog control

// ─── Project modules ───────────────────────────────────────
#include "config.h"

// Sensors
#include "sensors/heart_rate.h"
#include "sensors/mpu.h"
#include "sensors/gsr.h"
#include "sensors/rtc_clock.h"
#include "sensors/microphone.h"

// Actuators
#include "actuators/led_breathing.h"
#include "actuators/vibration.h"
#include "actuators/speaker.h"

// Display
#include "display/tft_display.h"

// Camera
#include "camera/camera.h"

// Network
#include "network/wifi_manager.h"
#include "network/web_server.h"
#include "network/openai_api.h"
#include "network/logger.h"

// Logic
#include "logic/sleep_detector.h"
#include "logic/command_handler.h"

// ─── Shared dashboard state ────────────────────────────────
static DashboardState dashState;

// ─── Speech task — runs on Core 0 ──────────────────────────
// Audio buffer lives in DRAM (not stack) — 64 KB
static int16_t audioBuffer[MIC_SAMPLE_RATE * MIC_RECORD_SECONDS];
static TaskHandle_t speechTaskHandle = nullptr;
static volatile bool speechBusy      = false;   // set while OpenAI call is in flight

// ─── Timing trackers ───────────────────────────────────────
static unsigned long lastDisplayTick   = 0;   // 1-second unified TFT + dashState tick
static unsigned long lastSRTime        = 0;   // speech recognition (Core 0 task)
static unsigned long lastVibrationTime = 0;
static unsigned long lastMovementTime  = 0;

// ─── Emergency state ───────────────────────────────────────
static bool emergencyFlag = false;

// ─── Sensor-available flags (graceful degradation) ─────────
static bool hasHeartRate = false;
static bool hasMPU       = false;
static bool hasRTC       = false;
static bool hasCamera    = false;

// ─── Forward declarations ──────────────────────────────────
static void onBreatheRequest();
static void onAlarmOn();
static void onAlarmOff();
static void onVibrateRequest();
static void speechTask(void* param);

// ============================================================
//  SETUP  (runs on Core 1)
// ============================================================
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    logInit(LOG_LEVEL_DEBUG);

    // ── Extend task watchdog timeout to 30 s ─────────────
    // The default is 5 s. The OpenAI HTTPS call (upload + inference)
    // can take 10-20 s. Without this the WDT kills the speech task
    // and reboots the chip every ~40 s.
    esp_task_wdt_init(30, true);   // 30 s timeout, panic on trigger

    LOG_INFO("MAIN", "M.I.N.D. Companion — Starting Up (Core %d)", xPortGetCoreID());

    // ── TFT Display ──────────────────────────────────────
    tftInit();
    tftShowBootScreen();

    // ── I2C bus (shared by MPU6050, MAX30102, RTC) ───────
    Wire.begin(I2C_SDA, I2C_SCL);

    // ── RTC ──────────────────────────────────────────────
    hasRTC = rtcInit();

    // ── Heart Rate Sensor ────────────────────────────────
    hasHeartRate = heartRateInit();
    if (!hasHeartRate) LOG_ERROR("MAIN", "Heart rate sensor FAILED — check I2C wiring");

    // ── MPU6050 ──────────────────────────────────────────
    hasMPU = mpuInit();
    if (!hasMPU) LOG_ERROR("MAIN", "MPU6050 FAILED");

    // ── GSR Sensor ───────────────────────────────────────
    gsrInit();

    // ── Button ───────────────────────────────────────────
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // ── LED Breathing ────────────────────────────────────
    ledBreathingInit();

    // ── Vibration Motor ──────────────────────────────────
    vibrationInit();

    // ── Speaker ──────────────────────────────────────────
    speakerInit();

    // ── Microphone ───────────────────────────────────────
    micInit();

    // ── Sleep Detector ───────────────────────────────────
    sleepDetectorInit();

    // ── WiFi ─────────────────────────────────────────────
    tftDrawDashboardLabels();
    tftUpdateStress("--", 0);

    if (wifiConnect()) {
        tftUpdateIP(wifiGetIP());
    } else {
        tftUpdateIP("No WiFi");
    }

    // ── Camera ───────────────────────────────────────────
    hasCamera = cameraInit();
    if (!hasCamera) {
        LOG_WARN("MAIN", "Camera unavailable — stream disabled");
    } else {
        cameraStartStreamServer();   // MJPEG on port 81 via esp_http_server
    }

    // ── Web Server ───────────────────────────────────────
    dashState.gsrOhms          = 0;
    dashState.stressLevel      = "Low";
    dashState.heartBPM         = 0;
    dashState.fingerPresent    = false;
    dashState.sleepQuality     = "Awake";
    dashState.emergencyActive  = false;
    dashState.accelX           = 0;
    dashState.accelY           = 0;
    dashState.accelZ           = 0;
    dashState.temperature      = 0;
    dashState.lastVoiceCommand = "";
    dashState.breathingActive  = false;

    webServerInit(&dashState);
    webServerSetCallbacks(onBreatheRequest, onAlarmOn, onAlarmOff, onVibrateRequest);

    // ── Init timing ──────────────────────────────────────
    unsigned long now  = millis();
    lastDisplayTick    = now;
    lastVibrationTime  = now;
    lastMovementTime   = now;

    // ── Speech task on Core 0 ────────────────────────────
    // Stack 8 KB — enough for HTTP + JSON parsing
    // Priority 1 (low) so Core 0 WiFi/BT tasks stay healthy
    xTaskCreatePinnedToCore(
        speechTask,         // function
        "speech",           // name
        8192,               // stack bytes
        nullptr,            // param
        1,                  // priority
        &speechTaskHandle,  // handle
        0                   // ← Core 0
    );

    // ── Ready ────────────────────────────────────────────
    speakerBeepOK();
    LOG_INFO("MAIN", "All systems go — sensor loop on Core 1, speech on Core 0");
}

// ============================================================
//  SPEECH TASK  (Core 0 — never touches sensors or TFT)
// ============================================================
static void speechTask(void* param) {
    for (;;) {
        // Wait until it's time for the next recognition cycle
        // vTaskDelay is preferred over delay() inside a task
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_SPEECH_RECOG));

        if (!wifiIsConnected()) continue;

        speechBusy = true;
        LOG_INFO("SPEECH", "Recording %d s...", MIC_RECORD_SECONDS);

        size_t bytesRead = micRecord(audioBuffer, sizeof(audioBuffer));

        if (bytesRead > 0) {
            size_t   pcmSize = bytesRead;
            size_t   wavSize = pcmSize + 44;
            uint8_t* wavData = (uint8_t*)malloc(wavSize);

            if (wavData) {
                micCreateWavHeader(wavData, pcmSize, MIC_SAMPLE_RATE);
                memcpy(wavData + 44, audioBuffer, pcmSize);

                LOG_INFO("SPEECH", "Sending %u bytes to OpenAI...", wavSize);
                String transcript = openaiTranscribe(wavData, wavSize);
                free(wavData);

                if (transcript.length() > 0) {
                    LOG_INFO("SPEECH", "Voice: %s", transcript.c_str());
                    // tftUpdateSpeechStatus touches the TFT — must be safe to call
                    // from Core 0. ILI9341 SPI is only driven from here + Core 1
                    // setup; by this point setup() is done so it's single-writer.
                    tftUpdateSpeechStatus(transcript);
                    dashState.lastVoiceCommand = transcript;

                    VoiceCommand cmd = commandParse(transcript);
                    switch (cmd) {
                        case CMD_BREATHING_PATTERN:
                            LOG_INFO("SPEECH", "→ breathing LED");
                            ledBreathingStart(5);
                            break;
                        case CMD_HELP_ME:
                        case CMD_EMERGENCY:
                            LOG_WARN("SPEECH", "→ voice emergency!");
                            speakerAlarmStart();
                            tftUpdateEmergency(true);
                            dashState.emergencyActive = true;
                            emergencyFlag = true;
                            break;
                        case CMD_HOW_AM_I:
                            speakerBeepOK();
                            break;
                        case CMD_STOP:
                            ledBreathingStop();
                            speakerAlarmStop();
                            tftUpdateEmergency(false);
                            dashState.emergencyActive = false;
                            emergencyFlag = false;
                            break;
                        default:
                            break;
                    }
                } else {
                    LOG_DEBUG("SPEECH", "No transcript returned");
                }
            } else {
                LOG_ERROR("SPEECH", "malloc failed for WAV buffer (%u bytes)", wavSize);
            }
        } else {
            LOG_WARN("SPEECH", "micRecord returned 0 bytes");
        }

        speechBusy = false;
    }
}

// ============================================================
//  LOOP  (Core 1 — sensors + display + web server)
//  Target: < 2 ms per iteration so heart rate FIFO never fills
//
//  Pattern:
//    • Sensors read EVERY iteration (no delay, keep FIFO drained)
//    • TFT + dashState update ONCE per second via lastDisplayTick
//    • tft.begin() / tftInit() called only once in setup() — never here
// ============================================================
void loop() {
    unsigned long now = millis();

    // =========================================================
    // A. SENSOR READS — every loop iteration, no gating
    //    These are fast (µs range) and must run continuously.
    // =========================================================

    // ── Heart Rate — drain MAX30102 FIFO every call ───────
    if (hasHeartRate) {
        heartRateUpdate();  // reads all queued samples, ~0.1 ms
    }

    // ── MPU6050 — continuous read for sleep detector ──────
    if (hasMPU) {
        mpuUpdate();
        sleepDetectorFeed(mpuGetMovementDelta());
        if (mpuMovementDetected(MOVEMENT_THRESHOLD)) {
            lastMovementTime = now;
        }
    }

    // ── Emergency Button — triggered ONLY by child pressing button ──
    // Latches on press. Does NOT auto-clear on release — stays active
    // until voice command "stop" or web dashboard clears it.
    static bool prevButtonState = false;
    bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);

if (buttonPressed && !prevButtonState) {   // just pressed

    emergencyFlag = !emergencyFlag;  // TOGGLE state

    if (emergencyFlag) {
        LOG_ERROR("MAIN", "EMERGENCY ACTIVATED");
        speakerAlarmStart();
        tftUpdateEmergency(true);
        dashState.emergencyActive = true;
    } 
    else {
        LOG_ERROR("MAIN", "EMERGENCY CLEARED");
        speakerAlarmStop();
        tftUpdateEmergency(false);
        dashState.emergencyActive = false;
    }
}

prevButtonState = buttonPressed;
    /*
    bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
    if (buttonPressed && !prevButtonState && !emergencyFlag) {
        // Rising edge (just pressed)
        emergencyFlag = true;
        LOG_ERROR("MAIN", "EMERGENCY BUTTON PRESSED");
        speakerAlarmStart();
        tftUpdateEmergency(true);
        dashState.emergencyActive = true;
    }
    prevButtonState = buttonPressed;
*/

    // ── No-Movement MPU Nudge (40 seconds) ───────────────
    // Does NOT set emergencyFlag — just beeps like a buzzer to
    // remind the child to move. Full alarm / emergency is button-only.
    static unsigned long lastNudgeBeep = 0;
    if (now - lastMovementTime >= NO_MOVEMENT_EMERGENCY_MS) {
        // Start a new nudge beep pattern every 5 seconds (only if not already beeping)
        if (now - lastNudgeBeep >= 5000UL && !speakerBuzzerIsActive()) {
            LOG_WARN("MAIN", "No movement for %lu s — nudge beep",
                     (now - lastMovementTime) / 1000UL);
            speakerBuzzerStart();
            lastNudgeBeep = now;
        }
    } else {
        lastNudgeBeep = 0;   // reset so beep fires immediately next time threshold is hit
    }

    // ── Non-blocking actuator state machines ─────────────
    dashState.breathingActive = ledBreathingUpdate();
    speakerAlarmUpdate();
    speakerBuzzerUpdate();

    // =========================================================
    // B. 1-SECOND DISPLAY + DASHBOARD TICK
    //    Everything that writes to the TFT or dashState lives
    //    here. Runs exactly once per second — no more, no less.
    //    tftInit() / tft.begin() are NEVER called here.
    // =========================================================
    if (now - lastDisplayTick >= 1000UL) {
        lastDisplayTick += 1000UL;   // advance by fixed step — prevents drift accumulation

        // ── RTC clock ─────────────────────────────────────
        if (hasRTC) {
            char timeBuf[9];
            rtcFormatTime(timeBuf, sizeof(timeBuf));
            tftUpdateTime(timeBuf);           // change-guarded inside
        }

        // ── Heart Rate ────────────────────────────────────
        if (hasHeartRate) {
            bool  finger = heartRateFingerPresent();
            int   bpm    = (int)heartRateGetBPM();
            tftUpdateHeartRate(bpm, finger);  // change-guarded inside
            dashState.heartBPM      = bpm;
            dashState.fingerPresent = finger;

            if (heartRateIsAbnormal() && !ledBreathingIsActive()) {
                LOG_WARN("MAIN", "Abnormal HR %d bpm — starting breathing LED", bpm);
                ledBreathingStart(5);
            }
        }

        // ── MPU / Motion ──────────────────────────────────
        if (hasMPU) {
            int ax = (int)mpuGetAccelX();
            int ay = (int)mpuGetAccelY();
            int az = (int)mpuGetAccelZ();
            tftUpdateMPU(ax, ay, az);         // change-guarded inside
            dashState.accelX      = mpuGetAccelX();
            dashState.accelY      = mpuGetAccelY();
            dashState.accelZ      = mpuGetAccelZ();
            dashState.temperature = mpuGetTemperature();

            String sq = sleepDetectorGetQuality();
            tftUpdateSleep(sq);               // change-guarded inside
            dashState.sleepQuality = sq;
        }

        // ── GSR Stress ────────────────────────────────────
        float  resistance = gsrReadResistance();
        String stress     = gsrGetStressLevel(resistance);
        tftUpdateStress(stress, resistance);  // change-guarded inside
        dashState.gsrOhms     = resistance;
        dashState.stressLevel = stress;
        LOG_DEBUG("GSR", "R=%.0f Ohm => %s", resistance, stress.c_str());

        // ── Hourly vibration reminder ─────────────────────
        if (now - lastVibrationTime >= INTERVAL_VIBRATION_HOUR) {
            if (!sleepDetectorIsSleeping()) {
                LOG_INFO("MAIN", "Hourly vibration reminder");
                vibrationPulse(1000);
            } else {
                LOG_INFO("MAIN", "Skipping vibration — user is sleeping");
            }
            lastVibrationTime = now;
        }
    }

    // ── yield — lets WiFi/BT stack run between iterations ─
    yield();
}

// ============================================================
//  Callbacks (web server remote control → Core 1 safe)
// ============================================================
static void onBreatheRequest() { ledBreathingStart(5); }
static void onAlarmOn()        { speakerAlarmStart(); }
static void onAlarmOff()       { speakerAlarmStop(); }
static void onVibrateRequest() { vibrationPulse(800); }
