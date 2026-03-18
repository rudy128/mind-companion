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
// Audio buffer allocated in PSRAM to avoid fragmenting DRAM
// (SSL handshake needs ~40KB contiguous DRAM)
static int16_t* audioBuffer = nullptr;
static const size_t AUDIO_BUFFER_SIZE = MIC_SAMPLE_RATE * MIC_RECORD_SECONDS * sizeof(int16_t);
static TaskHandle_t speechTaskHandle = nullptr;
static volatile bool speechBusy      = false;   // set while OpenAI call is in flight
static volatile bool speechTrigger   = false;   // set by double-press, cleared by task

// ─── Timing trackers ───────────────────────────────────────
static unsigned long lastDisplayTick   = 0;   // 1-second unified TFT + dashState tick
static unsigned long lastMovementTime  = 0;
static unsigned long awakeStartTime    = 0;   // when "Awake" state began
static bool          awakeNudgeFired   = false; // true after the 1-min nudge has fired

// ─── Button state for single/double press detection ───────
static unsigned long lastPressTime    = 0;
static uint8_t       pressCount       = 0;
static bool          buttonWasPressed = false;

// ─── Emergency state ───────────────────────────────────────────
static bool emergencyFlag = false;

// Differentiates sleep-triggered emergency from hardware-button emergency.
//   true  → deep-sleep alert: speaker loops ALARM_AUDIO_FILE
//   false → button / voice emergency: speaker stays SILENT
// Set ONLY by the 90-second deep-sleep detector. Cleared by dashboard.
static bool sleepEmergency = false;

// ─── Manual Alarm state ───────────────────────────────────────
static bool manualAlarmActive = false;

// Tracks how long the device has been continuously in Deep Sleep.
// Reset to 0 whenever the sleep state changes away from "Deep Sleep".
static unsigned long deepSleepStartTime = 0;

// ─── Breathing session timer ──────────────────────────────────
// 0 = indefinite (HR-triggered or dashboard cmd)
// non-zero = millis() timestamp at which the voice session expires
static unsigned long breathingEndMs = 0;

// ─── Awake-nudge camera window ────────────────────────────────
// Set to millis()+20000 when the awake nudge fires.
// dashState.cameraOpen is driven by this value every 1-second tick.
static unsigned long camOpenUntilMs = 0;

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
    // ALLOCATE AUDIO BUFFER IN PSRAM (keeps DRAM free for SSL)
    // ══════════════════════════════════════════════════════
    audioBuffer = (int16_t*)ps_malloc(AUDIO_BUFFER_SIZE);
    if (!audioBuffer) {
        Serial.println("[FATAL] Failed to allocate audio buffer in PSRAM!");
        // Fallback to regular malloc if PSRAM fails
        audioBuffer = (int16_t*)malloc(AUDIO_BUFFER_SIZE);
    }
    Serial.printf("[MEM] Audio buffer: %u bytes in %s\n", 
                  AUDIO_BUFFER_SIZE, 
                  esp_ptr_external_ram(audioBuffer) ? "PSRAM" : "DRAM");

    // ══════════════════════════════════════════════════════
    // AUDIO FIRST — spawns task on Core 0 for smooth playback
    // ══════════════════════════════════════════════════════
    audioQuotesInit();
    
    // Give the audio task time to start
    delay(100);
    
    // Test play
    audioQuotesTestPlay();
    
    // Let it play for a bit
    delay(3000);

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
    // Init only — breathing starts automatically when HR is abnormal
    // (in the 1-second tick below) or via voice / dashboard command.
    ledBreathingInit();

    // ── Vibration Motor ──────────────────────────────────
    vibrationInit();

    // ── Microphone ───────────────────────────────────────
    // DON'T init mic here — it conflicts with Audio library on I2S0
    // Mic driver is installed on-demand during recording (time-sliced)
    // micInit();  // REMOVED - causes I2S conflict at startup
    LOG_INFO("MIC", "Microphone ready (on-demand I2S, time-sliced with speaker)");

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
    lastMovementTime   = now;
    awakeStartTime     = 0;  // Not awake until detected

    // ── Speech task on Core 0 — waits for double-press trigger ──
    // Time-sliced I2S: audio pauses during recording
    xTaskCreatePinnedToCore(
        speechTask,         // function
        "speech",           // name
        49152,              // stack bytes (48 KB)
        nullptr,            // param
        1,                  // priority
        &speechTaskHandle,  // handle
        0                   // ← Core 0
    );

    // ── Ready ────────────────────────────────────────────
    LOG_INFO("MAIN", "All systems go — sensor loop on Core 1");
}

// ============================================================
//  SPEECH TASK  (Core 0 — event-driven, waits for trigger)
//  Triggered by double-press on button. Records, transcribes,
//  then goes back to sleep. Uses time-sliced I2S with speaker.
// ============================================================
static void speechTask(void* param) {
    LOG_INFO("SPEECH", "Task started on Core 0 — waiting for trigger");
    
    for (;;) {
        // Sleep until triggered by double-press (check every 100ms)
        while (!speechTrigger) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // Clear trigger immediately
        speechTrigger = false;
        speechBusy = true;
        
        LOG_INFO("SPEECH", "--- RECORDING TRIGGERED ---");

        if (!wifiIsConnected()) {
            LOG_WARN("SPEECH", "WiFi not connected — aborting");
            speechBusy = false;
            continue;
        }

        // Verify the TCP/IP stack is actually healthy (DNS probe)
        if (!wifiEnsureConnected()) {
            LOG_WARN("SPEECH", "WiFi stack dead — aborting");
            speechBusy = false;
            continue;
        }

        // ══════════════════════════════════════════════════════
        // PAUSE AUDIO — Release I2S for microphone
        // ══════════════════════════════════════════════════════
        audioQuotesPause();
        vTaskDelay(pdMS_TO_TICKS(200));  // Let I2S fully release

        LOG_INFO("SPEECH", "Recording %d s (buf=%u bytes)...", 
                 MIC_RECORD_SECONDS, AUDIO_BUFFER_SIZE);

        tftShowListening(true);
        size_t bytesRead = micRecord(audioBuffer, AUDIO_BUFFER_SIZE);
        tftShowListening(false);

        // ══════════════════════════════════════════════════════
        // RESUME AUDIO — Re-initialize I2S for speaker
        // ══════════════════════════════════════════════════════
        audioQuotesResume();

        LOG_INFO("SPEECH", "micRecord returned %u bytes", bytesRead);

        if (bytesRead > 0) {
            LOG_INFO("SPEECH", "Sending to OpenAI Whisper...");
            String transcript = openaiTranscribe(audioBuffer, bytesRead);
            LOG_INFO("SPEECH", "Transcript: \"%s\"", transcript.c_str());

            if (transcript.length() == 0) {
                wifiSetLastCycleFailed(true);
                LOG_WARN("SPEECH", "Empty transcript — failed");
            } else {
                wifiSetLastCycleFailed(false);
                tftUpdateSpeechStatus(transcript);
                
                xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
                strlcpy(dashState.lastVoiceCommand, transcript.c_str(), 
                        sizeof(dashState.lastVoiceCommand));
                xSemaphoreGive(mqttDashMutex);

                // Parse and execute command
                VoiceCommand cmd = commandParse(transcript);
                LOG_INFO("SPEECH", "Command: %s", commandToString(cmd).c_str());

                xSemaphoreTake(actuatorMutex, portMAX_DELAY);
                switch (cmd) {
                    case CMD_BREATHING_PATTERN:
                        // Start the continuous fade worker; the orchestration
                        // timer (breathingEndMs) will stop it after 35 seconds.
                        ledBreathingStart();
                        breathingEndMs = millis() + 35000UL;
                        LOG_INFO("SPEECH", "Voice breathing pattern — will run for 35 s");
                        break;
                    case CMD_HELP_ME:
                    case CMD_EMERGENCY:
                        emergencyFlag = true;
                        tftUpdateEmergency(true);
                        xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
                        dashState.emergencyActive = true;
                        xSemaphoreGive(mqttDashMutex);
                        mqttPublishAlert(true);
                        break;
                    case CMD_STOP:
                        ledBreathingStop();
                        emergencyFlag = false;
                        tftUpdateEmergency(false);
                        xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
                        dashState.emergencyActive = false;
                        xSemaphoreGive(mqttDashMutex);
                        mqttPublishAlert(false);
                        break;
                    default:
                        break;
                }
                xSemaphoreGive(actuatorMutex);
            }
        } else {
            LOG_WARN("SPEECH", "No audio recorded");
        }

        LOG_INFO("SPEECH", "--- DONE --- heap=%lu", esp_get_free_heap_size());
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
    // ── Audio runs on its own Core 0 task — no audioQuotesLoop() call needed ──

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

    // =========================================================
    // BUTTON HANDLING — Double press = voice (priority), Single = emergency
    // Logic: Count presses within 1s window. If 2+ presses detected,
    // trigger voice immediately. If only 1 press after timeout, emergency.
    // =========================================================
    bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
    
    // Detect falling edge (button just pressed, active LOW)
    if (buttonPressed && !buttonWasPressed) {
        // Debounce check
        if (now - lastPressTime > DEBOUNCE_MS) {
            pressCount++;
            lastPressTime = now;
            
            // ══════════════════════════════════════════════════
            // INSTANT DOUBLE-PRESS DETECTION (priority)
            // If this is the 2nd press, trigger voice immediately
            // ══════════════════════════════════════════════════
            if (pressCount >= 2) {
                if (!speechBusy) {
                    LOG_INFO("BUTTON", "DOUBLE PRESS — triggering voice recording");
                    speechTrigger = true;
                    vibrationPulse(100);  // Short feedback buzz
                } else {
                    LOG_WARN("BUTTON", "Speech already in progress");
                }
                pressCount = 0;  // Reset immediately after handling
            }
        }
    }
    buttonWasPressed = buttonPressed;
    
    // After timeout, if only 1 press was registered → emergency toggle
    if (pressCount == 1 && (now - lastPressTime > DOUBLE_PRESS_WINDOW_MS)) {
        // ══════════════════════════════════════════════════
        // SINGLE PRESS (after timeout) → Toggle emergency
        // ══════════════════════════════════════════════════
        xSemaphoreTake(actuatorMutex, portMAX_DELAY);
        emergencyFlag = !emergencyFlag;
        
        if (emergencyFlag) {
            LOG_ERROR("MAIN", "EMERGENCY ACTIVATED (single press)");
            tftUpdateEmergency(true);
            xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
            dashState.emergencyActive = true;
            xSemaphoreGive(mqttDashMutex);
            mqttPublishAlert(true);
        } else {
            LOG_INFO("MAIN", "EMERGENCY CLEARED (single press)");
            sleepEmergency = false;
            if (audioQuotesIsPlaying()) audioQuotesStop();
            deepSleepStartTime = 0;
            tftUpdateEmergency(false);
            xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
            dashState.emergencyActive = false;
            xSemaphoreGive(mqttDashMutex);
            mqttPublishAlert(false);
        }
        xSemaphoreGive(actuatorMutex);
        
        pressCount = 0;  // Reset for next detection
    }

    // ── Non-blocking actuator state machines ─────────────────
    xSemaphoreTake(actuatorMutex, portMAX_DELAY);

    // Orchestration timer: gracefully stop a timed breathing session once expired.
    // The LED finishes its current fade-out cycle then turns off — no abrupt cut.
    // HR-triggered sessions have breathingEndMs == 0 and are stopped by the
    // 1-second HR normalisation check further below.
    if (breathingEndMs > 0 && millis() >= breathingEndMs && ledBreathingIsActive()) {
        ledBreathingGracefulStop();
        breathingEndMs = 0;
        LOG_INFO("MAIN", "Breathing session ending (graceful fade-out)");
    }

    bool breathingNow = ledBreathingUpdate();
    vibrationUpdate();

    // ── Sleep-emergency and Manual alarm audio loop ─────────────
    // playFileLooped is idempotent — only triggers once, then the audio
    // system handles continuous replay internally until audioQuotesStop().
    if ((sleepEmergency && emergencyFlag) || manualAlarmActive) {
        playFileLooped(ALARM_AUDIO_FILE);
    }

    xSemaphoreGive(actuatorMutex);

    xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
    dashState.breathingActive = breathingNow;
    dashState.micActive       = speechBusy;
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
            int   bpm    = (int)(heartRateGetBPM() + 0.5f);  // round for display
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

            // ── Auto-start breathing on abnormal HR ─────────────────
            // Don't override an active voice-triggered session (breathingEndMs > 0).
            if (heartRateIsAbnormal()) {
                xSemaphoreTake(actuatorMutex, portMAX_DELAY);
                if (!ledBreathingIsActive()) {
                    ledBreathingStart();   // indefinite — no end time set
                    breathingEndMs = 0;
                    LOG_WARN("MAIN", "Abnormal HR %d bpm — starting breathing LED", bpm);
                }
                xSemaphoreGive(actuatorMutex);
            } else {
                // HR is normal: stop only if this is an HR-triggered (indefinite)
                // session. Leave a voice-triggered session (breathingEndMs > 0) running.
                xSemaphoreTake(actuatorMutex, portMAX_DELAY);
                if (ledBreathingIsActive() && breathingEndMs == 0) {
                    ledBreathingStop();
                    LOG_INFO("MAIN", "HR normalised (%d bpm) — stopping breathing LED", bpm);
                }
                xSemaphoreGive(actuatorMutex);
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

            // ── Awake periodic nudge: vibrate 2 s + camera 20 s, repeats every minute ──
            //
            // Logic:
            //   • Track when user first becomes Awake (awakeStartTime).
            //   • Once they have been Awake for ≥60 s, fire the nudge:
            //       - Vibrate for 2 s (short haptic reminder).
            //       - Set camOpenUntilMs = now + 20 s  (camera window opens on dashboard).
            //       - Reset awakeNudgeFired so the 60-s clock restarts — nudge repeats
            //         every minute as long as the user stays Awake.
            //   • dashState.cameraOpen is driven by camOpenUntilMs every 1-s tick,
            //     independent of the nudge trigger.
            if (sq == "Awake") {
                if (awakeStartTime == 0) awakeStartTime = now;

                if ((now - awakeStartTime >= 60000UL) && !awakeNudgeFired) {
                    LOG_INFO("MAIN", "Awake ≥60 s nudge: vibrating 2 s, camera on for 20 s");
                    xSemaphoreTake(actuatorMutex, portMAX_DELAY);
                    vibrationPulse(2000);   // 2-second haptic nudge
                    xSemaphoreGive(actuatorMutex);
                    camOpenUntilMs  = now + 20000UL;  // camera window: 20 s
                    awakeNudgeFired = true;            // arms the 60-s reset below
                    awakeStartTime  = now;             // restart 60-s clock for next nudge
                }
                // Once the nudge fired, wait for the camera window to expire,
                // then clear awakeNudgeFired so the next 60-s cycle can trigger.
                if (awakeNudgeFired && camOpenUntilMs > 0 && now >= camOpenUntilMs) {
                    awakeNudgeFired = false;  // ready to fire again next minute
                }

            } else {  // Light Sleep / Deep Sleep / Restless
                awakeStartTime  = 0;
                awakeNudgeFired = false;
                // We do NOT clear camOpenUntilMs here so manual dashboard 
                // triggers during sleep aren't instantly cancelled.
            }

            // Drive dashState.cameraOpen from camOpenUntilMs window
            bool camShouldBeOpen = (camOpenUntilMs > 0 && now < camOpenUntilMs);
            if (dashState.cameraOpen != camShouldBeOpen) {
                xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
                dashState.cameraOpen = camShouldBeOpen;
                xSemaphoreGive(mqttDashMutex);
                LOG_INFO("MAIN", "Camera %s", camShouldBeOpen ? "ON (awake window)" : "OFF");
            }

            // ── Deep-Sleep Emergency ────────────────────────────────────
            // If the user stays in Deep Sleep for ≥90 seconds, trigger an
            // emergency AND start looping alarm audio (ALARM_AUDIO_FILE).
            // This is a SEPARATE mode from the hardware-button emergency:
            //   • sleepEmergency = true  → speaker plays alarm file in a loop
            //   • sleepEmergency = false → button/voice emergency — silent
            // Only the dashboard (clear_emergency) can stop this.
            if (!emergencyFlag) {         // don’t stack on top of an active emergency
                if (sq == "Deep Sleep") {
                    if (deepSleepStartTime == 0) deepSleepStartTime = now;

                    if ((now - deepSleepStartTime >= 90000UL) && !sleepEmergency) {
                        LOG_ERROR("MAIN", "User in Deep Sleep \u226590 s — triggering sleep emergency");
                        emergencyFlag  = true;
                        sleepEmergency = true;
                        tftUpdateEmergency(true);
                        xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
                        dashState.emergencyActive = true;
                        xSemaphoreGive(mqttDashMutex);
                        mqttPublishAlert(true);
                        playFileLooped(ALARM_AUDIO_FILE);
                    }
                } else {
                    // Not in Deep Sleep — reset the pending timer
                    deepSleepStartTime = 0;
                }
            } else {
                // If we've already triggered a *sleep* emergency, clear it as soon
                // as the user leaves Deep Sleep (to any other state).
                //
                // Keep manual alarm/button emergency independent.
                if (sleepEmergency && !manualAlarmActive && sq != "Deep Sleep") {
                    LOG_INFO("MAIN", "Deep Sleep emergency cleared (sleep state changed)");
                    emergencyFlag  = false;
                    sleepEmergency = false;
                    audioQuotesStop();
                    deepSleepStartTime = 0;
                    tftUpdateEmergency(false);
                    xSemaphoreTake(mqttDashMutex, portMAX_DELAY);
                    dashState.emergencyActive = false;
                    xSemaphoreGive(mqttDashMutex);
                    mqttPublishAlert(false);
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
static void onVibrateRequest() {
    vibrationPulse(2000);
    camOpenUntilMs = millis() + 20000UL;
    
    // Reset the Awake cycle trackers so they don't clash with this manual trigger.
    // By setting awakeNudgeFired=true, the main loop will let the 20s camera window
    // play out, then wait another 60s from now before auto-firing an Awake nudge.
    awakeStartTime = millis();
    awakeNudgeFired = true;
    
    LOG_INFO("MAIN", "Manual vibrate request: 2s vibe, 20s camera, resetting awake cycle");
}
static void onClearEmergency()   {
    emergencyFlag     = false;
    sleepEmergency    = false;
    manualAlarmActive = false;
    audioQuotesStop();
    deepSleepStartTime = 0;   // allow re-trigger if user goes back to deep sleep
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
        breathingEndMs = millis() + 35000UL;
        LOG_INFO("MQTT_CMD", "Breathing LED started — 35 s session");
    } else if (cmd == "alarm_on") {
        if (!speechBusy) {
            // Stop any active audio (e.g. sleep-emergency loop) so we
            // get a clean handoff — no two sources fighting over the speaker.
            audioQuotesStop();
            manualAlarmActive = true;
            playFileLooped(ALARM_AUDIO_FILE);
            LOG_INFO("MQTT_CMD", "Manual alarm ON — took over speaker");
        } else {
            LOG_WARN("MQTT_CMD", "Ignored alarm_on (mic active)");
        }
    } else if (cmd == "alarm_off") {
        manualAlarmActive = false;
        // Always stop — if sleep emergency is still active, the loop()
        // block will detect it on the next iteration and re-init the
        // emergency audio cycle from scratch.
        audioQuotesStop();
        LOG_INFO("MQTT_CMD", "Manual alarm OFF — speaker released");
    } else if (cmd == "vibrate") {
        onVibrateRequest();
    } else if (cmd == "clear_emergency") {
        onClearEmergency();
    } else {
        LOG_WARN("MQTT_CMD", "Unknown command: %s", cmd.c_str());
    }
    xSemaphoreGive(actuatorMutex);
}