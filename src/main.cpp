// ============================================================
//  M.I.N.D. Companion — Main Orchestrator
//  ESP32-S3 WROOM (Freenove) • Arduino Framework • PlatformIO
// =============================================================
//
//  Core split:
//    Core 1 (loop) — sensors, TFT display, actuators
//                    Runs every ~1 ms with no blocking waits.
//    Core 0 (tasks) — MQTT task (50 ms tick, 1 Hz publish)
//                     Speech task (when enabled — recording + OpenAI)
//                     WiFi event task (ESP-IDF internal)
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
#include "actuators/audio_quotes.h"

// Display
#include "display/tft_display.h"

// Camera
#include "camera/camera.h"

// Network
#include "network/wifi_manager.h"
#include "network/mqtt_client.h"
#include "network/openai_api.h"
#include "network/logger.h"

// Logic
#include "logic/sleep_detector.h"
#include "logic/command_handler.h"

// ─── Shared dashboard state (published via MQTT) ───────────
static MqttDashState dashState;

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
static unsigned long awakeStartTime    = 0;   // when "Awake" state began
static unsigned long lastAwakeNudge    = 0;   // last awake-nudge vibration
static bool          awakeNudgeFired   = false; // true after the 1-min nudge has fired

// ─── Emergency state ───────────────────────────────────────
static bool emergencyFlag = false;

// ─── Actuator mutex — protects actuator start/stop/update + emergencyFlag
//     from races between Core 0 (MQTT command callback) and Core 1 (loop).
//     Lock ordering: actuatorMutex → mqttDashMutex (never reversed).
static SemaphoreHandle_t actuatorMutex = nullptr;

// ─── Sensor-available flags (graceful degradation) ─────────
static bool hasHeartRate = false;
static bool hasMPU       = false;
static bool hasRTC       = false;
static bool hasCamera    = false;

// ─── Forward declarations ──────────────────────────────────
static void onVibrateRequest();
static void onClearEmergency();
static void speechTask(void* param);
static void onMqttCommand(const String& cmd);

// ============================================================
//  SETUP  (runs on Core 1)
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    // ══════════════════════════════════════════════════════
    // AUDIO FIRST — before anything else touches I2S/SPI
    // ══════════════════════════════════════════════════════
    audioQuotesInit();
    audioQuotesTestPlay();
    
    // Let audio play for a bit before other init
    for (int i = 0; i < 500; i++) {
        audioQuotesLoop();
        delay(10);
    }

    logInit(LOG_LEVEL_DEBUG);

    // ── Extend task watchdog timeout to 30 s ─────────────
    // The default is 5 s. The OpenAI HTTPS call (upload + inference)
    // can take 10-20 s. Without this the WDT kills the speech task
    // and reboots the chip every ~40 s.
    esp_task_wdt_init(30, true);   // 30 s timeout, panic on trigger

    LOG_INFO("MAIN", "M.I.N.D. Companion — Starting Up (Core %d)", xPortGetCoreID());

    // ── Create actuator mutex ────────────────────────────
    actuatorMutex = xSemaphoreCreateMutex();

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
    ledBreathingStart(); 

    // ── Vibration Motor ──────────────────────────────────
    vibrationInit();

    // ── Microphone ───────────────────────────────────────
    // micInit();   // DISABLED — testing sensors + MQTT first

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

    // ── MQTT Client (replaces web server) ──────────────────
    // ── MQTT Client (replaces web server) ──────────────────
    dashState.gsrValue         = 0;
    strlcpy(dashState.stressLevel, "Low", sizeof(dashState.stressLevel));
    dashState.heartBPM         = 0;
    dashState.fingerPresent    = false;
    strlcpy(dashState.sleepQuality, "Awake", sizeof(dashState.sleepQuality));
    dashState.emergencyActive  = false;
    dashState.accelX           = 0;
    dashState.accelY           = 0;
    dashState.accelZ           = 0;
    dashState.temperature      = 0;
    strlcpy(dashState.lastVoiceCommand, "", sizeof(dashState.lastVoiceCommand));
    dashState.breathingActive  = false;
    dashState.cameraOpen       = false;

    mqttInit(&dashState);
    mqttSetCommandCallback(onMqttCommand);

    // ── Init timing ──────────────────────────────────────
    unsigned long now  = millis();
    lastDisplayTick    = now;
    lastVibrationTime  = now;
    lastMovementTime   = now;
    awakeStartTime     = now;
    lastAwakeNudge     = now;

    // ── Speech task on Core 0 ────────────────────────────
    // DISABLED — testing sensors + MQTT stability first.
    // The OpenAI TLS calls on Core 0 starve the WiFi task and cause
    // MQTT keepalive timeouts. Re-enable once MQTT is confirmed stable.
    /*
    xTaskCreatePinnedToCore(
        speechTask,         // function
        "speech",           // name
        49152,              // stack bytes (48 KB)
        nullptr,            // param
        1,                  // priority
        &speechTaskHandle,  // handle
        0                   // ← Core 0
    );
    */

    // ── Ready ────────────────────────────────────────────
    LOG_INFO("MAIN", "All systems go — sensor loop on Core 1");
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

        // Verify the TCP/IP stack is actually healthy (DNS probe).
        // After an SSL abort WiFi stays "connected" but DNS dies.
        if (!wifiEnsureConnected()) {
            LOG_WARN("SPEECH", "WiFi stack dead — skipping cycle");
            continue;
        }

        speechBusy = true;
        LOG_INFO("SPEECH", "--- CYCLE START --- WiFi=%s heap=%lu",
                 wifiIsConnected() ? "OK" : "DOWN", esp_get_free_heap_size());
        LOG_INFO("SPEECH", "Recording %d s (buf=%u bytes)...", MIC_RECORD_SECONDS, sizeof(audioBuffer));

        tftShowListening(true);
        size_t bytesRead = micRecord(audioBuffer, sizeof(audioBuffer));
        tftShowListening(false);

        LOG_INFO("SPEECH", "micRecord returned %u bytes (expected %u)",
                 bytesRead, sizeof(audioBuffer));

        if (bytesRead > 0) {
            // Stream PCM directly to OpenAI — no malloc copy needed.
            // The WAV header is built inside openaiTranscribe() on the stack.
            size_t pcmSize = bytesRead;
            LOG_INFO("SPEECH", "Sending %u PCM bytes to OpenAI Whisper...", pcmSize);
            String transcript = openaiTranscribe(audioBuffer, pcmSize);
            LOG_INFO("SPEECH", "OpenAI returned: \"%s\" (len=%u)",
                     transcript.c_str(), transcript.length());

            // Signal WiFi manager whether this cycle succeeded or failed.
            // It takes multiple consecutive failures before a force-reconnect
            // is attempted — a single SSL timeout is normal and shouldn't
            // tear down WiFi for the web server and camera.
            if (transcript.length() == 0) {
                wifiSetLastCycleFailed(true);
                LOG_WARN("SPEECH", "Transcription failed — will retry next cycle");
            } else {
                wifiSetLastCycleFailed(false);
            }

            if (transcript.length() > 0) {
                tftUpdateSpeechStatus(transcript);
                xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
                strlcpy(dashState.lastVoiceCommand, transcript.c_str(), sizeof(dashState.lastVoiceCommand));
                xSemaphoreGive(mqttDashMutex);

                LOG_INFO("SPEECH", "Parsing command from transcript...");
                VoiceCommand cmd = commandParse(transcript);
                LOG_INFO("SPEECH", "Command parsed: %s (%d)", commandToString(cmd).c_str(), (int)cmd);

                switch (cmd) {
                    case CMD_BREATHING_PATTERN:
                        LOG_INFO("SPEECH", "→ CMD_BREATHING_PATTERN: calling ledBreathingStart(5)");
                        //ledBreathingStart(5);   from old pattern
                        LOG_INFO("SPEECH", "→ ledBreathingStart done, isActive=%s",
                                 ledBreathingIsActive() ? "true" : "false");
                        break;
                    case CMD_HELP_ME:
                    case CMD_EMERGENCY:
                        LOG_WARN("SPEECH", "→ CMD_EMERGENCY: activating emergency");
                        xSemaphoreTake(actuatorMutex, portMAX_DELAY);
                        emergencyFlag = true;
                        xSemaphoreGive(actuatorMutex);
                        tftUpdateEmergency(true);
                        xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
                        dashState.emergencyActive = true;
                        xSemaphoreGive(mqttDashMutex);
                        LOG_WARN("SPEECH", "→ Emergency active");
                        break;
                    case CMD_HOW_AM_I:
                        LOG_INFO("SPEECH", "→ CMD_HOW_AM_I: status check");
                        // Could play a status audio file here
                        break;
                    case CMD_STOP:
                        LOG_INFO("SPEECH", "→ CMD_STOP: stopping LED + emergency");
                        xSemaphoreTake(actuatorMutex, portMAX_DELAY);
                        ledBreathingStop();
                        emergencyFlag = false;
                        xSemaphoreGive(actuatorMutex);
                        tftUpdateEmergency(false);
                        xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
                        dashState.emergencyActive = false;
                        xSemaphoreGive(mqttDashMutex);
                        break;
                    default:
                        LOG_DEBUG("SPEECH", "→ CMD_NONE — no keyword matched");
                        break;
                }
            } else {
                LOG_WARN("SPEECH", "OpenAI returned empty transcript — nothing to parse");
            }
        } else {
            LOG_WARN("SPEECH", "micRecord returned 0 bytes — skipping OpenAI call");
        }

        LOG_INFO("SPEECH", "--- CYCLE END --- heap=%lu", esp_get_free_heap_size());
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

    // ── DEBUG: heartbeat every 10s to prove loop() runs ──
    static unsigned long lastHeartbeat = 0;
    static unsigned long loopCount = 0;
    loopCount++;
    if (now - lastHeartbeat >= 10000UL) {
        lastHeartbeat = now;
        Serial.printf("[LOOP] %lu ms — alive loops=%lu bpm=%d finger=%d mqtt=%d\n",
                      now, loopCount, dashState.heartBPM,
                      (int)dashState.fingerPresent, mqttIsConnected());
        loopCount = 0;
    }

    // ── MQTT runs on its own Core 0 task — no mqttLoop() call here ──

    // ── Audio Quotes Loop — process MP3 playback ─────────
    audioQuotesLoop();
    
    //dashState.breathingActive = ledBreathingUpdate();
    // =========================================================
    // A. SENSOR READS — every loop iteration, no gating
    //    These are fast (µs range) and must run continuously.
    // =========================================================

    // ── Heart Rate — simple direct read every loop iteration ───
    // The simple getIR() approach (no FIFO drain) needs to be
    // called as frequently as possible for reliable beat detection.
    if (hasHeartRate) {
        heartRateUpdate();
    }

    // ── MPU6050 — read every 50ms for sleep detector ──────
    // No need to poll every loop iteration — 20 Hz is plenty
    // for movement detection and sleep analysis, and it frees
    // the I2C bus for the heart rate sensor.
    static unsigned long lastMpuRead = 0;
    if (hasMPU && (now - lastMpuRead >= 50UL)) {
        lastMpuRead = now;
        mpuUpdate();
        if (mpuMovementDetected(MOVEMENT_THRESHOLD)) {
            lastMovementTime = now;
        }
    }

    // ── RTC clock — polled once per second via millis() to avoid
    //    hammering the shared I2C bus. TFT updates the instant the
    //    second changes, so no drift from the 1-s display tick.
    if (hasRTC) {
        static unsigned long lastRtcPoll = 0;
        static uint8_t       prevSecond  = 255;
        if (now - lastRtcPoll >= 950UL) {   // poll ~50 ms before expected tick
            lastRtcPoll = now;
            uint8_t curSecond = rtcGetSecond();
            if (curSecond != prevSecond) {
                prevSecond = curSecond;
                char timeBuf[9];
                rtcFormatTime(timeBuf, sizeof(timeBuf));
                tftUpdateTime(timeBuf);
            }
        }
    }

    // ── Emergency Button — triggered ONLY by child pressing button ──
    // Latches on press. Does NOT auto-clear on release — stays active
    // until voice command "stop" or web dashboard clears it.
    static bool prevButtonState = false;
    bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
    

if (buttonPressed && !prevButtonState) {   // just pressed

    xSemaphoreTake(actuatorMutex, portMAX_DELAY);
    emergencyFlag = !emergencyFlag;  // TOGGLE state

    if (emergencyFlag) {
        LOG_ERROR("MAIN", "EMERGENCY ACTIVATED");
        // Could play an alarm MP3: playAudioFile("/alarm.mp3");
        tftUpdateEmergency(true);
        xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
        dashState.emergencyActive = true;
        xSemaphoreGive(mqttDashMutex);
        mqttPublishAlert(true);
    } 
    else {
        LOG_ERROR("MAIN", "EMERGENCY CLEARED");
        tftUpdateEmergency(false);
        xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
        dashState.emergencyActive = false;
        xSemaphoreGive(mqttDashMutex);
        mqttPublishAlert(false);
    }
    xSemaphoreGive(actuatorMutex);
}

prevButtonState = buttonPressed;

    // ── Non-blocking actuator state machines ─────────────
    xSemaphoreTake(actuatorMutex, portMAX_DELAY);
    bool breathingNow = ledBreathingUpdate();
    vibrationUpdate();
    xSemaphoreGive(actuatorMutex);

    xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
    dashState.breathingActive = breathingNow;
    xSemaphoreGive(mqttDashMutex);

    // =========================================================
    // B. 1-SECOND DISPLAY + DASHBOARD TICK
    //    Everything that writes to the TFT or dashState lives
    //    here. Runs exactly once per second — no more, no less.
    //    tftInit() / tft.begin() are NEVER called here.
    // =========================================================
    if (now - lastDisplayTick >= 1000UL) {
        lastDisplayTick = now;   // snap to current time — prevents multiple catch-up fires

        Serial.printf("[TICK] %lu ms — 1s tick firing\n", now);

        // ── Heart Rate ────────────────────────────────────
        if (hasHeartRate) {
            bool  finger = heartRateFingerPresent();
            int   bpm    = (int)heartRateGetBPM();
            tftUpdateHeartRate(bpm, finger);  // change-guarded inside
            xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
            dashState.heartBPM      = bpm;
            dashState.fingerPresent = finger;
            xSemaphoreGive(mqttDashMutex);

            Serial.printf("[TICK-HR] bpm=%d finger=%d\n", bpm, (int)finger);

            // DEBUG: confirm dashState is being updated for MQTT
            static int lastLoggedBpm = -1;
            static bool lastLoggedFinger = false;
            if (bpm != lastLoggedBpm || finger != lastLoggedFinger) {
                LOG_INFO("DASH", "dashState updated — bpm=%d finger=%s",
                         bpm, finger ? "true" : "false");
                lastLoggedBpm = bpm;
                lastLoggedFinger = finger;
            }

            if (heartRateIsAbnormal() && !ledBreathingIsActive()) {
                //LOG_WARN("MAIN", "Abnormal HR %d bpm — starting breathing LED", bpm);
                //ledBreathingStart(5);
            }
        }

        // ── MPU / Motion ──────────────────────────────────
        if (hasMPU) {
            int dispX = (int)mpuGetAccelX();
            int dispY = (int)mpuGetAccelY();
            int dispZ = (int)mpuGetAccelZ();
            tftUpdateMPU(dispX, dispY, dispZ); // change-guarded inside
            xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
            dashState.accelX      = mpuGetAccelX();
            dashState.accelY      = mpuGetAccelY();
            dashState.accelZ      = mpuGetAccelZ();
            dashState.temperature = mpuGetTemperature();
            xSemaphoreGive(mqttDashMutex);

            // Per-axis deltas for sleep detector (computed in mpu.cpp)
            float dx = mpuGetDeltaX();
            float dy = mpuGetDeltaY();
            float dz = mpuGetDeltaZ();
            int activeAxes = mpuGetActiveAxes(0.3f);

            // Feed one sample per second into the 30-sample ring buffer
            sleepDetectorFeed(dx, dy, dz, activeAxes);

            String sq = sleepDetectorGetQuality();
            tftUpdateSleep(sq);               // change-guarded inside
            xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
            strlcpy(dashState.sleepQuality, sq.c_str(), sizeof(dashState.sleepQuality));
            xSemaphoreGive(mqttDashMutex);

            // ── Awake nudge — vibrate + open camera every 60 s ───
            // Tracks how long the child has been continuously awake.
            // Resets the awake-start timer as soon as they fall asleep.
            /*if (sq == "Awake") {
                if (now - lastAwakeNudge >= INTERVAL_AWAKE_NUDGE) {
                    lastAwakeNudge = now;
                    unsigned long awakeSeconds = (now - awakeStartTime) / 1000UL;
                    LOG_INFO("MAIN", "Awake nudge — awake for %lu s", awakeSeconds);
                    vibrationPulse(800);
                    dashState.cameraOpen = true;   // dashboard will open camera for 20 s
                }
            } else {
                // Child fell asleep — reset both timers
                awakeStartTime = now;
                lastAwakeNudge = now;
                dashState.cameraOpen = false;
            }*/
           // ── State-based vibration + camera control ─────────────
            // ── Awake 1-minute trigger: vibration + camera (fires ONCE) ──
            if (sq == "Awake") {

                // Start timer when user first becomes awake
                if (awakeStartTime == 0) {
                    awakeStartTime = now;
                }

                // Only trigger ONCE after 1 minute of continuous awake
                if ((now - awakeStartTime >= 60000UL) && !awakeNudgeFired) {
                    LOG_INFO("MAIN", "User awake ≥ 1 min — Vibrating + Camera ON");
                    vibrationPulse(300);         // short non-disruptive pulse
                    xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
                    dashState.cameraOpen = true; // Turn camera ON (one-shot for dashboard)
                    xSemaphoreGive(mqttDashMutex);
                    awakeNudgeFired = true;      // don't fire again until user sleeps
                }

            } else {  // Light Sleep / Deep Sleep

                // Reset awake timer & nudge flag
                awakeStartTime  = 0;
                awakeNudgeFired = false;
                if (dashState.cameraOpen) {
                    LOG_INFO("MAIN", "User sleeping — Camera OFF");
                    xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
                    dashState.cameraOpen = false;
                    xSemaphoreGive(mqttDashMutex);
                }
            }
        }


        // ── GSR Stress ────────────────────────────────────
        float  conductance = gsrReadConductance();
        String stress      = gsrGetStressLevel(conductance);
        tftUpdateStress(stress, conductance);
        xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
        dashState.gsrValue    = conductance;
        strlcpy(dashState.stressLevel, stress.c_str(), sizeof(dashState.stressLevel));
        xSemaphoreGive(mqttDashMutex);

        // ── GSR High-stress → Play audio quote ──
        // GSR triggers audio quote playback when high stress is detected.
        // The alarm is reserved for the emergency button (CMD_EMERGENCY).
        if (stress == "High") {
            LOG_WARN("GSR", "HIGH STRESS DETECTED (%.1f < %.0f)",
                     conductance, (float)GSR_MODERATE_THRESHOLD);
            // Play calming quote directly (non-blocking)
            static unsigned long lastQuoteTime = 0;
            if (now - lastQuoteTime >= 30000UL) {  // Max once every 30 seconds
                playQuoteByCategory(QUOTE_CALM);
                lastQuoteTime = now;
                LOG_INFO("GSR", "Playing calming audio quote due to high stress");
            }
        }

        // ── Hourly vibration reminder ─────────────────────
        /*if (now - lastVibrationTime >= INTERVAL_VIBRATION_HOUR) {
            if (!sleepDetectorIsSleeping()) {
                LOG_INFO("MAIN", "Hourly vibration reminder");
                vibrationPulse(1000);
            } else {
                LOG_INFO("MAIN", "Skipping vibration — user is sleeping");
            }
            lastVibrationTime = now;
        }*/
    }

    // ── yield — lets WiFi/BT stack run between iterations ─
    // delay(1) yields for at least 1 ms which is enough for the WiFi
    // task on Core 0 to service TCP packets and TLS handshakes.
    // Without this, Core 1 hogs the CPU and SSL connections time out.
    delay(1);
}


// ============================================================
//  Callbacks (web server remote control → Core 1 safe)
// ============================================================
static void onVibrateRequest()   { vibrationPulse(800); }
static void onClearEmergency()   {
    emergencyFlag = false;
    tftUpdateEmergency(false);
    xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
    dashState.emergencyActive = false;
    xSemaphoreGive(mqttDashMutex);
    mqttPublishAlert(false);
    LOG_INFO("MAIN", "Emergency cleared via dashboard");
}

// ============================================================
//  MQTT Command Handler — receives commands from dashboard
//  Commands arrive as JSON on mind/cmd: {"cmd":"breathe"}
// ============================================================
static void onMqttCommand(const String& cmd) {
    LOG_INFO("MQTT_CMD", "Received command: %s", cmd.c_str());

    xSemaphoreTake(actuatorMutex, portMAX_DELAY);
    if (cmd == "breathe") {
        ledBreathingStart();
    } else if (cmd == "vibrate") {
        onVibrateRequest();
    } else if (cmd == "clear_emergency") {
        onClearEmergency();
    } else {
        LOG_WARN("MQTT_CMD", "Unknown command: %s", cmd.c_str());
    }
    xSemaphoreGive(actuatorMutex);
}