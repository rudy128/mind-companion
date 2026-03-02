# M.I.N.D. Companion — Complete Technical Documentation

> **M**onitoring **I**ntelligent **N**eurological **D**evice  
> An ESP32-S3 wearable health companion for children, featuring real-time physiological monitoring, AI-powered voice recognition, sleep detection, stress analysis, and remote parent dashboard.

---

## Table of Contents

1. [Hardware Platform](#1-hardware-platform)
2. [Build System & Dependencies](#2-build-system--dependencies)
3. [System Architecture](#3-system-architecture)
4. [Boot Sequence](#4-boot-sequence)
5. [Dual-Core Execution Model](#5-dual-core-execution-model)
6. [Sensor Modules](#6-sensor-modules)
   - 6.1 [Heart Rate (MAX30102)](#61-heart-rate-sensor--max30102)
   - 6.2 [Accelerometer/Gyroscope (MPU6050)](#62-accelerometergyroscope--mpu6050)
   - 6.3 [Galvanic Skin Response (GSR)](#63-galvanic-skin-response--gsr)
   - 6.4 [Real-Time Clock (DS3231)](#64-real-time-clock--ds3231)
   - 6.5 [Microphone (INMP441)](#65-microphone--inmp441)
7. [Actuator Modules](#7-actuator-modules)
   - 7.1 [LED Breathing Pattern](#71-led-breathing-pattern)
   - 7.2 [Vibration Motor](#72-vibration-motor)
   - 7.3 [Speaker (MAX98357A)](#73-speaker--max98357a)
8. [Camera Module (OV2640)](#8-camera-module--ov2640)
9. [Logic Modules](#9-logic-modules)
   - 9.1 [Sleep Detector](#91-sleep-detector)
   - 9.2 [Voice Command Handler](#92-voice-command-handler)
10. [Display Module (ILI9341 TFT)](#10-display-module--ili9341-tft)
11. [Network Modules](#11-network-modules)
    - 11.1 [WiFi Manager](#111-wifi-manager)
    - 11.2 [OpenAI Whisper API](#112-openai-whisper-api)
    - 11.3 [Web Server & Dashboard](#113-web-server--dashboard)
    - 11.4 [Centralized Logger](#114-centralized-logger)
12. [Main Loop — Complete Execution Flow](#12-main-loop--complete-execution-flow)
13. [Emergency System](#13-emergency-system)
14. [Awake Nudge System](#14-awake-nudge-system)
15. [Pin Mapping Reference](#15-pin-mapping-reference)
16. [Timing Constants Reference](#16-timing-constants-reference)
17. [Threshold Constants Reference](#17-threshold-constants-reference)
18. [File Structure](#18-file-structure)

---

## 1. Hardware Platform

| Component | Part | Interface | Notes |
|---|---|---|---|
| **MCU** | Freenove ESP32-S3 WROOM N8R8 | — | 240 MHz dual-core Xtensa, 320 KB SRAM, 8 MB Flash (QIO), 8 MB PSRAM (OPI) |
| **Heart Rate** | MAX30102 | I2C (0x57) | Pulse oximeter, IR/Red LED, on shared I2C bus (GPIO47/48) |
| **IMU** | MPU6050 | I2C (0x69) | 3-axis accelerometer + gyroscope, AD0 pulled HIGH, on shared I2C bus |
| **RTC** | DS3231 | I2C | Battery-backed real-time clock, on shared I2C bus |
| **GSR** | Grove GSR module | ADC (GPIO3) | Voltage divider with 10 kΩ reference resistor |
| **Microphone** | INMP441 | I2S #0 RX | MEMS microphone, 16 kHz 16-bit mono |
| **Speaker** | MAX98357A | I2S #1 TX | Class-D amplifier, 22.05 kHz 16-bit mono |
| **Display** | ILI9341 | Software SPI | 240×320 TFT, 5-wire (CS, DC, MOSI, SCK, RST) |
| **Camera** | OV2640 | DVP (parallel 8-bit) | Built into Freenove board, QVGA JPEG (320×240), 2 frame buffers in PSRAM |
| **LED** | 4× LEDs via PNP transistor | PWM (GPIO20) | 5 kHz, 8-bit resolution (0–255) |
| **Vibration** | DC vibration motor | Digital (GPIO46) | Simple HIGH/LOW control |
| **Button** | Tactile push-button | Digital (GPIO41) | Active LOW with internal pull-up |

All three I2C devices (MAX30102, MPU6050, DS3231) share a single I2C bus on GPIO47 (SDA) / GPIO48 (SCL). The camera uses its own SCCB bus on GPIO4 (SDA) / GPIO5 (SCL) which is separate from the sensor I2C bus.

---

## 2. Build System & Dependencies

**File:** `platformio.ini`

```ini
[env:freenove_esp32_s3_wroom]
platform   = espressif32
board      = freenove_esp32_s3_wroom
framework  = arduino
```

**Key build settings:**
- `board_build.partitions = huge_app.csv` — enlarged app partition to fit camera + all libraries.
- `build_flags = -mfix-esp32-psram-cache-issue` — required PSRAM cache fix for ESP32-S3.
- `board_build.filesystem = littlefs` — filesystem type (not actively used for serving pages; dashboard HTML is embedded in code).

**Library dependencies (from `lib_deps`):**

| Library | Version | Purpose |
|---|---|---|
| `bblanchon/ArduinoJson` | ^7.4.2 | JSON serialization for API responses and OpenAI parsing |
| `adafruit/Adafruit MPU6050` | ^2.2.9 | MPU6050 accelerometer/gyro driver |
| `adafruit/Adafruit ILI9341` | ^1.6.3 | TFT display driver |
| `adafruit/RTClib` | ^2.1.4 | DS3231 RTC driver |
| `sparkfun/SparkFun MAX3010x` | ^1.1.2 | MAX30102 heart rate sensor driver |
| `espressif/esp32-camera` | ^2.0.4 | OV2640 camera driver |
| `esp32async/ESPAsyncWebServer` | ^3.10.0 | Asynchronous web server for dashboard |
| `esp32async/AsyncTCP` | ^3.4.10 | TCP backend for async web server |

---

## 3. System Architecture

```
┌───────────────────────────────────────────────────────────────────┐
│                         ESP32-S3 DUAL CORE                        │
│                                                                   │
│  ┌──────────────────────────┐   ┌──────────────────────────────┐ │
│  │     CORE 1 (loop)        │   │      CORE 0 (speechTask)     │ │
│  │                          │   │                              │ │
│  │  Every iteration (~1ms): │   │  Every 15s:                  │ │
│  │   • heartRateUpdate()    │   │   1. vTaskDelay(15s)         │ │
│  │   • mpuUpdate()          │   │   2. micRecord() — 2s        │ │
│  │   • RTC poll (950ms)     │   │   3. Build WAV header        │ │
│  │   • Button check         │   │   4. HTTPS → OpenAI Whisper  │ │
│  │   • Nudge beep check     │   │   5. Parse transcript        │ │
│  │   • Actuator updates     │   │   6. commandParse()          │ │
│  │                          │   │   7. Execute command          │ │
│  │  Every 1s (tick B):      │   │                              │ │
│  │   • TFT + dashState      │   │  Never touches sensors or    │ │
│  │   • HR display/abnormal  │   │  TFT layout directly (only   │ │
│  │   • MPU display + deltas │   │  tftUpdateSpeechStatus).     │ │
│  │   • sleepDetectorFeed()  │   │                              │ │
│  │   • Awake nudge logic    │   │  Isolated so HTTPS blocking  │ │
│  │   • GSR stress           │   │  (10–20s) never stalls       │ │
│  │   • Hourly vibration     │   │  sensor reads.               │ │
│  └──────────────────────────┘   └──────────────────────────────┘ │
│                                                                   │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │                    SHARED RESOURCES                           │ │
│  │  • DashboardState struct — written by both cores              │ │
│  │  • _tftMutex (FreeRTOS semaphore) — protects SPI bus          │ │
│  │  • I2C bus — used by Core 1 only (sensors)                    │ │
│  │  • WiFi stack — used by Core 0 (OpenAI) + AsyncWebServer     │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                                                                   │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │                    NETWORK SERVERS                            │ │
│  │  Port 80 — ESPAsyncWebServer (dashboard + JSON API)           │ │
│  │  Port 81 — esp_http_server (MJPEG camera stream)              │ │
│  └──────────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────────┘
```

The two web servers are deliberately different frameworks:
- **Port 80** uses `ESPAsyncWebServer` — non-blocking, event-driven, ideal for dashboard pages and JSON API endpoints.
- **Port 81** uses `esp_http_server` (ESP-IDF native) — the MJPEG stream handler is an infinite loop that sends multipart JPEG frames continuously. Using the native server prevents this infinite loop from blocking AsyncWebServer's event loop.

---

## 4. Boot Sequence

Defined in `setup()` in `main.cpp`, runs on **Core 1**:

```
1.  Serial.begin(115200)
2.  logInit(LOG_LEVEL_DEBUG)       — initialize ring-buffer logger
3.  esp_task_wdt_init(30, true)    — extend watchdog to 30s (default 5s)
4.  tftInit()                      — ILI9341 SPI init, create _tftMutex
5.  tftShowBootScreen()            — 2.5s splash with sensor list
6.  Wire.begin(47, 48)             — start shared I2C bus
7.  rtcInit()                      — DS3231 RTC
8.  heartRateInit()                — MAX30102 (400Hz, 69µs, 4096 ADC)
9.  mpuInit()                      — MPU6050 (±2g, ±250°/s, 21Hz filter)
10. gsrInit()                      — GPIO3 ADC, 12-bit resolution
11. pinMode(BUTTON_PIN, INPUT_PULLUP)
12. ledBreathingInit()             — PWM channel 0, 5kHz, 8-bit
13. vibrationInit()                — GPIO46 output
14. speakerInit()                  — I2S #1 TX, 22.05kHz
15. micInit()                      — I2S #0 RX, 16kHz
16. sleepDetectorInit()            — zero both ring buffers
17. tftDrawDashboardLabels()       — permanent TFT section headers
18. wifiConnect()                  — blocking connect with 30 retries
19. cameraInit()                   — OV2640, QVGA JPEG, 2 FB in PSRAM
20. cameraStartStreamServer()      — port 81 MJPEG server
21. webServerInit(&dashState)      — port 80, register all routes
22. webServerSetCallbacks(...)     — register 5 callback function pointers
23. Initialize all timing variables to millis()
24. xTaskCreatePinnedToCore(speechTask, ..., Core 0)  — 8KB stack, priority 1
25. speakerBeepOK()                — two short beeps to confirm ready
```

Each sensor init returns a boolean stored in `hasHeartRate`, `hasMPU`, `hasRTC`, `hasCamera`. If any sensor fails, the system continues running without it (graceful degradation). The failed sensor's data simply shows `--` on the TFT and dashboard.

---

## 5. Dual-Core Execution Model

### Core 1 — `loop()` (main.cpp)

The main loop runs at approximately 1 ms per iteration with **no blocking waits**. It is split into two sections:

**Section A — Every Iteration (no gating):**
- `heartRateUpdate()` — drains the MAX30102 FIFO. Must run every iteration or the 32-sample FIFO fills with stale data and beat detection lags.
- `mpuUpdate()` — reads latest accelerometer/gyro/temp via I2C. Stores current values and saves previous values for delta computation.
- `mpuMovementDetected(0.2)` — checks if any axis changed by > 0.2 m/s². If so, resets `lastMovementTime`.
- RTC poll — rate-limited to every 950 ms via `millis()`. Reads `rtcGetSecond()` and only updates TFT when the second value changes. This catches the second boundary within ~50 ms without hammering I2C (which would happen at thousands of reads/sec if unchecked).
- Emergency button — reads GPIO41 (active LOW). Detects rising edge (press). Toggles `emergencyFlag` — first press activates emergency, second press clears it.
- No-movement nudge — if `lastMovementTime` was > 40 seconds ago, plays a PCM buzzer sound via the speaker every 5 seconds. This is a gentle reminder, not a full emergency.
- Actuator state machines — `ledBreathingUpdate()`, `speakerAlarmUpdate()`, `speakerBuzzerUpdate()` are called every iteration. They are non-blocking: each call either does nothing (if inactive) or advances their internal state by one step.

**Section B — 1-Second Tick (gated by `lastDisplayTick`):**

Triggered when `now - lastDisplayTick >= 1000UL`. The tick counter advances by a fixed 1000 ms step (`lastDisplayTick += 1000UL`) instead of being set to `now`, preventing drift accumulation.

Inside this tick:
- Heart rate: read BPM and finger status, update TFT and dashState. If BPM is abnormal (>120 or <50) and breathing LED is not already active, start breathing pattern.
- MPU: display X/Y/Z on TFT. Compute per-axis deltas (`mpuGetDeltaX/Y/Z()`) and active axis count (`mpuGetActiveAxes(0.3)`). Feed one sample into the sleep detector ring buffer.
- Sleep detector: get quality string, update TFT and dashState. Run awake nudge logic (see §14).
- GSR: read conductance, classify stress level, update TFT and dashState.
- Hourly vibration: if enough time has passed AND the user is not sleeping, pulse the vibration motor.

### Core 0 — `speechTask()` (main.cpp)

A FreeRTOS task pinned to Core 0, stack size 8192 bytes, priority 1. Runs an infinite loop:

```
1. vTaskDelay(15000ms)             — sleep 15 seconds
2. Check wifiIsConnected()         — skip cycle if offline
3. Set speechBusy = true
4. micRecord(audioBuffer, 64000)   — record 2s at 16kHz 16-bit = 64000 bytes
5. malloc WAV buffer (64044 bytes) — 44-byte WAV header + PCM data
6. micCreateWavHeader()            — write RIFF/WAVE header
7. openaiTranscribe(wavData, wavSize) — HTTPS POST to api.openai.com
8. If transcript received:
     a. tftUpdateSpeechStatus()    — show on TFT (mutex-protected)
     b. dashState.lastVoiceCommand = transcript
     c. commandParse(transcript)   — keyword matching
     d. Execute matched command (breathing LED, emergency, stop, etc.)
9. free(wavData)
10. Set speechBusy = false
```

**Why Core 0?** The OpenAI HTTPS call takes 10-20 seconds (TLS handshake + upload + inference). If this ran on Core 1, the MAX30102 FIFO would overflow and all sensors would stall. Isolating it on Core 0 ensures Core 1's sub-millisecond loop timing is never disrupted.

**Watchdog:** The default FreeRTOS task WDT is 5 seconds. Since OpenAI calls take longer, `esp_task_wdt_init(30, true)` extends it to 30 seconds. Additionally, `esp_task_wdt_reset()` is called during the upload loop and response-reading loop in `openai_api.cpp`.

---

## 6. Sensor Modules

### 6.1 Heart Rate Sensor — MAX30102

**Files:** `src/sensors/heart_rate.h`, `src/sensors/heart_rate.cpp`

**Hardware config (set in `heartRateInit()`):**
| Parameter | Value | Reason |
|---|---|---|
| Sample rate | 400 Hz | New sample every 2.5 ms |
| Pulse width | 69 µs | Fastest, lowest latency |
| ADC range | 4096 | Highest sensitivity |
| LED brightness | 0x3F | Strong enough for finger detection |
| Sample average | 1 | No hardware averaging — raw samples |
| LED mode | 2 | Red + IR (SpO2 mode, though we only use IR) |
| I2C speed | 400 kHz | Fast mode for lower read latency |

**Beat detection algorithm (`heartRateUpdate()`):**
1. Drain the entire MAX30102 FIFO in a single call — `while (particleSensor.available())`. This prevents data staleness when the main loop slows down (e.g., during SPI display writes).
2. Read `irValue` from each sample.
3. Finger detection: `irValue > 10000` means finger is present.
4. On finger placement: call `resetRollingAverage()` to clear stale data. This zeroes out the 4-sample BPM buffer.
5. Run `checkForBeat(irValue)` — this is SparkFun's library function that detects peaks in the IR signal.
6. On beat detected: compute `currentBPM = 60.0 / (delta_ms / 1000.0)`. Only accept values in range [20, 255] BPM.
7. Store in 4-slot rolling average array `rates[4]`. Compute average BPM from non-zero entries.
8. On finger removal: `resetRollingAverage()` — clears everything so next placement starts fresh.

**Abnormal HR detection:** `heartRateIsAbnormal()` returns `true` if `averageBPM > 120` or `< 50` (and finger is present). This triggers the breathing LED pattern in main.cpp.

### 6.2 Accelerometer/Gyroscope — MPU6050

**Files:** `src/sensors/mpu.h`, `src/sensors/mpu.cpp`

**Hardware config (set in `mpuInit()`):**
- I2C address: `0x69` (AD0 pin pulled HIGH to avoid collision with MAX30102 at 0x57)
- Accelerometer range: ±2g
- Gyroscope range: ±250°/s
- Digital low-pass filter: 21 Hz bandwidth

**Data flow:**
```
mpuUpdate() called every loop iteration (~1ms):
  1. mpu.getEvent(&a, &g, &t)     — read all sensor data in one I2C transaction
  2. Save previous: lastAx=ax, lastAy=ay, lastAz=az
  3. Update current: ax, ay, az, gx, gy, gz, temp
```

**Movement detection functions:**
- `mpuMovementDetected(threshold)` — returns `true` if any single axis delta exceeds `threshold` (used with 0.2 m/s² for no-movement nudge).
- `mpuGetDeltaX/Y/Z()` — returns `fabsf(current - previous)` for each axis. Used by the sleep detector.
- `mpuGetActiveAxes(threshold)` — counts how many axes have delta > `threshold` (used with 0.3 m/s²). Returns 0–3.
- `mpuGetMovementDelta()` — returns the magnitude difference between current and previous readings.

### 6.3 Galvanic Skin Response — GSR

**Files:** `src/sensors/gsr.h`, `src/sensors/gsr.cpp`

**Circuit:** A voltage divider between the GSR sensor module and a 10 kΩ reference resistor. The ESP32's 12-bit ADC reads the midpoint on GPIO3.

**Reading algorithm (`gsrReadConductance()`):**
1. Take 10 ADC samples in a tight loop (`GSR_SAMPLES = 10`).
2. Compute average: `avg = sum / 10.0`.
3. Clamp minimum to 1 (prevent division by zero).
4. Compute conductance-proportional value: `R_REF × (ADC_MAX / avg - 1)` = `10000 × (4095 / avg - 1)`.

The formula outputs higher values when skin is more conductive (more sweat = lower resistance = higher value). Despite the mathematical form resembling a resistance calculation, the signal direction is: **higher output → more stress**.

**Stress classification (`gsrGetStressLevel(float c)`):**
| Range | Level | TFT Color |
|---|---|---|
| c > 24000 | "High" | Red |
| c ≥ 14000 | "Moderate" | Yellow |
| c ≥ 7900 | "Low" | Green |
| c < 7900 | "Please wear finger band" | Red (2-line text) |

**Observed real-world readings:**
- No finger on sensor: ~7000–8000
- Dry finger: ~12000–14000
- Sweaty/stressed: ~20000+

### 6.4 Real-Time Clock — DS3231

**Files:** `src/sensors/rtc_clock.h`, `src/sensors/rtc_clock.cpp`

The DS3231 is a battery-backed RTC with ±2 ppm accuracy. It shares the I2C bus with MPU6050 and MAX30102.

**Functions:**
- `rtcInit()` — calls `rtc.begin()`. Returns false if DS3231 not found.
- `rtcFormatTime(buf, len)` — reads current time via `rtc.now()` and formats as `"HH:MM:SS"` using `snprintf`.
- `rtcGetHour/Minute/Second()` — individual component getters. Each calls `rtc.now()` internally (one I2C read per call).
- `rtcSetToCompileTime()` — sets RTC to the compile timestamp. Called once during initial flash, then commented out.

**How it's polled in main.cpp (Section A):**

The RTC is NOT read inside the 1-second tick. It's polled in every loop iteration, but rate-limited to every 950 ms:

```cpp
static unsigned long lastRtcPoll = 0;
static uint8_t       prevSecond  = 255;
if (now - lastRtcPoll >= 950UL) {
    lastRtcPoll = now;
    uint8_t curSecond = rtcGetSecond();
    if (curSecond != prevSecond) {
        prevSecond = curSecond;
        rtcFormatTime(timeBuf, sizeof(timeBuf));
        tftUpdateTime(timeBuf);
    }
}
```

This catches the second boundary within ~50 ms (since we poll 50 ms before the expected tick). Without rate limiting, the I2C bus would be hammered thousands of times per second. Tying it to the 1-second display tick would cause visible 2-5 second jumps when the tick drifts due to SPI/WiFi blocking.

### 6.5 Microphone — INMP441

**Files:** `src/sensors/microphone.h`, `src/sensors/microphone.cpp`

**I2S configuration (set in `micInit()`):**
| Parameter | Value |
|---|---|
| I2S port | I2S_NUM_0 (RX) |
| Sample rate | 16000 Hz |
| Bits per sample | 16-bit |
| Channel format | Left only (mono) |
| DMA buffers | 8 × 1024 bytes |

**Recording algorithm (`micRecord(buffer, bufferSize)`):**

The function fills a caller-provided buffer with PCM audio data. A single `i2s_read()` call may return fewer bytes than requested (DMA boundary), so it loops:

```
totalRead = 0
while (totalRead < bufferSize && retries < 40):
    i2s_read(port, buffer+totalRead, remaining, &bytesRead, 100ms timeout)
    if bytesRead > 0: totalRead += bytesRead
    else: retries++
    vTaskDelay(1)   // feed watchdog
return totalRead
```

- Maximum retry: 40 × 100 ms = 4 seconds (covers 2s audio + I2S latency).
- Buffer size for 2 seconds: `16000 × 2 × 2 = 64000 bytes` (16-bit samples).
- The audio buffer is a global static array in DRAM (not stack-allocated): `static int16_t audioBuffer[MIC_SAMPLE_RATE * MIC_RECORD_SECONDS]`.

**WAV header generation (`micCreateWavHeader(wav, pcmSize, sampleRate)`):**

Writes a 44-byte RIFF/WAVE header at the start of the buffer:
- Format: PCM, 1 channel, 16-bit
- Byte rate: sampleRate × 2
- The header is followed immediately by the raw PCM data.

**Important limitation:** The microphone records a 2-second snapshot every ~15-30 seconds (15s delay + 2s recording + 10-15s OpenAI round-trip). It is NOT always listening. Speech that occurs outside the 2-second recording window is completely missed.

---

## 7. Actuator Modules

### 7.1 LED Breathing Pattern

**Files:** `src/actuators/led_breathing.h`, `src/actuators/led_breathing.cpp`

Drives 4 LEDs through a PNP transistor on GPIO20 using ESP32 LEDC PWM (channel 0, 5 kHz, 8-bit resolution).

**State machine (`ledBreathingUpdate()`):**

Non-blocking — called every loop iteration. Uses `millis()` timing:

```
Every 15ms (STEP_MS):
  if rising:
    brightness += 5
    if brightness >= 255: switch to falling
  else:
    brightness -= 5
    if brightness <= 0: switch to rising, currentCycle++
    if currentCycle >= totalCycles: stop, write 0 to PWM
  ledcWrite(0, brightness)
```

One full cycle (inhale + exhale) = 255/5 × 15ms × 2 = ~1.53 seconds. Default is 5 cycles (~7.6 seconds total).

**Trigger conditions:**
1. Voice command "breathing" or "breathe" (via Core 0 speech task)
2. Abnormal heart rate detected (>120 or <50 BPM)
3. Dashboard "Start" button (`/api/breathe` endpoint)

### 7.2 Vibration Motor

**Files:** `src/actuators/vibration.h`, `src/actuators/vibration.cpp`

Simple digital output on GPIO46. `vibrationPulse(durationMs)` is a **blocking** call — it sets GPIO HIGH, calls `delay(durationMs)`, then sets GPIO LOW.

**Trigger conditions:**
1. Hourly vibration reminder (if not sleeping) — `INTERVAL_VIBRATION_HOUR` (currently 60000 ms = 1 minute for testing)
2. Awake nudge — every 60 seconds while sleep detector reports "Awake"
3. Dashboard "Pulse" button (`/api/vibrate` endpoint)

Note: Because `vibrationPulse` is blocking, an 800ms pulse temporarily stalls the Core 1 loop. For the current use cases (infrequent, < 1s), this is acceptable. A non-blocking rewrite would require a state machine similar to `ledBreathingUpdate()`.

### 7.3 Speaker — MAX98357A

**Files:** `src/actuators/speaker.h`, `src/actuators/speaker.cpp`, `src/actuators/buzzer_pcm.h`

Uses I2S port #1 in TX mode at 22.05 kHz, 16-bit mono. The MAX98357A is a Class-D amplifier that converts the I2S digital signal to analog speaker output.

**Three sound modes:**

#### 1. Tone generation (`speakerPlayTone(freqHz, durationMs)`)
Generates a sine wave in software:
```cpp
buf[i] = (int16_t)(15000 * sinf(2π × freq × i / sampleRate));
```
Writes 256-sample chunks to I2S with 50ms timeout per chunk. Followed by a silence buffer to prevent amplifier clicks.

Used by: `speakerBeepOK()` (880 Hz + 1100 Hz) and `speakerBeepError()` (440 Hz).

#### 2. Alarm pattern (`speakerAlarmStart/Stop/Update()`)
A non-blocking two-tone siren:
- Alternates between 1200 Hz and 800 Hz every 400 ms.
- Each tone burst is 100 ms (short enough to not block the loop significantly).
- `speakerAlarmUpdate()` must be called every loop iteration.

Triggered by: emergency button press, voice command "help" or "emergency".
Stopped by: voice command "stop", dashboard dismiss button, second button press.

#### 3. PCM buzzer (`speakerBuzzerStart/Stop/Update()`)
Streams a pre-recorded PCM waveform from `buzzer_pcm.h` — a 2765-element `int16_t` array stored in flash (PROGMEM). This is a digitized buzzer sound from `assets/buzzer_trim.wav`.

- Streams 256 samples per `speakerBuzzerUpdate()` call.
- Total duration: ~2765 / 22050 ≈ 125 ms.
- Auto-stops when all samples are sent.

Triggered by: no-movement nudge (40 seconds without MPU delta > 0.2). Plays once every 5 seconds while the child remains still.

---

## 8. Camera Module — OV2640

**Files:** `src/camera/camera.h`, `src/camera/camera.cpp`

**Initialization (`cameraInit()`):**

```
Camera config:
  XCLK frequency: 20 MHz
  Pixel format:   JPEG
  Frame size:     QVGA (320×240)
  JPEG quality:   12 (lower = better quality, higher file size)
  Frame buffers:  2 (in PSRAM if available, DRAM fallback with 1 buffer)
  Grab mode:      CAMERA_GRAB_LATEST (skip stale frames)
  LEDC channel:   1 (channel 0 is used by LED PWM)
```

Post-init sensor adjustments: vertical flip OFF, brightness +1, saturation 0.

**MJPEG Stream Server (`cameraStartStreamServer()`):**

Starts a separate `esp_http_server` instance on port 81 (ctrl port 32769 to avoid clashing with AsyncWebServer).

The stream handler is an infinite loop:
```cpp
while (true) {
    fb = esp_camera_fb_get();            // grab JPEG frame
    httpd_resp_send_chunk(BOUNDARY);     // "\r\n--frame\r\n"
    httpd_resp_send_chunk(PART_HEADER);  // "Content-Type: image/jpeg\r\nContent-Length: N\r\n\r\n"
    httpd_resp_send_chunk(fb->buf);      // JPEG data
    esp_camera_fb_return(fb);            // return buffer to pool
}
```

CORS header `Access-Control-Allow-Origin: *` is set to allow the dashboard on port 80 to load the stream from port 81.

**Dashboard integration:** The camera feed is hidden by default (`display:none`). It becomes visible for 20 seconds when:
1. The user clicks "Pulse" (vibration) on the dashboard — `triggerVibrate()` JS function calls `/api/vibrate` then `openCamera(20)`.
2. The awake nudge fires — `dashState.cameraOpen` is set to `true`, dashboard JS detects it and calls `openCamera(20)`.

The `openCamera(seconds)` JS function sets `img.src` to the stream URL (starting the MJPEG connection), shows a countdown timer, and auto-closes (sets `img.src = ''`) after timeout.

---

## 9. Logic Modules

### 9.1 Sleep Detector

**Files:** `src/logic/sleep_detector.h`, `src/logic/sleep_detector.cpp`

**Algorithm:** 30-sample ring buffer with 1 sample per second = 30-second sliding window.

**Data structures:**
```cpp
static float deltaBuffer[30];   // total per-axis delta per sample
static float axesBuffer[30];    // active axis count per sample (for logging)
static int   bufferIndex = 0;
static bool  bufferFull = false;
```

**Feed function (`sleepDetectorFeed(dx, dy, dz, activeAxes)`):**

Called once per second from the 1-second tick in main.cpp. Parameters are computed by main.cpp using `mpuGetDeltaX/Y/Z()` and `mpuGetActiveAxes(0.3)`.

```
1. totalDelta = dx + dy + dz
2. Store totalDelta in deltaBuffer[bufferIndex]
3. Store activeAxes in axesBuffer[bufferIndex]  (for serial logging only)
4. Advance bufferIndex (wraps at 30)
5. If buffer not full yet: return (no classification until 30 samples)
6. Compute avgDelta = sum(deltaBuffer) / 30
7. Compute avgActiveAxes = sum(axesBuffer) / 30  (logged, not used for classification)
```

**Classification thresholds (calibrated from real hardware observations):**

| Condition | Classification | Real-world meaning |
|---|---|---|
| `avgDelta > 1.5` | **Awake** | Strong, sustained movement |
| `avgDelta > 0.10` | **Light Sleep** | Weak or intermittent fidgeting |
| `avgDelta ≤ 0.10` | **Deep Sleep** | Essentially motionless |

**Observed real-world values:**
| State | avgDelta | avgAxes |
|---|---|---|
| Full board movement | 2.5 – 3.5 | 1.5 – 2.0 |
| Still on desk | 0.03 – 0.06 | 0.00 |

Note: `avgActiveAxes` is NOT used for classification. On this hardware, the MPU6050's gravity vector always contributes to Z-axis, so even vigorous shaking only yields ~1.8 active axes (never reliably 3). The `avgDelta` magnitude is the sole discriminator.

### 9.2 Voice Command Handler

**Files:** `src/logic/command_handler.h`, `src/logic/command_handler.cpp`

Simple keyword-matching parser. Converts the OpenAI transcript to lowercase, then checks for substrings:

| Keywords | Command Enum | Action |
|---|---|---|
| "breathing", "breathe" | `CMD_BREATHING_PATTERN` | Start 5-cycle LED breathing |
| "how am i", "status" | `CMD_HOW_AM_I` | Play OK beep |
| "help", "help me" | `CMD_HELP_ME` | Activate emergency |
| "emergency", "panic" | `CMD_EMERGENCY` | Activate emergency |
| "stop", "cancel" | `CMD_STOP` | Stop LED, alarm, clear emergency |

Keywords are checked in priority order (first match wins). `CMD_NONE` is returned if no keyword matches.

---

## 10. Display Module — ILI9341 TFT

**Files:** `src/display/tft_display.h`, `src/display/tft_display.cpp`

**Hardware:** 240×320 pixel ILI9341 TFT connected via **software SPI** (not hardware SPI) on GPIO1 (SCK), GPIO2 (MOSI), GPIO39 (CS), GPIO42 (DC), GPIO40 (RST). Rotation 0 (portrait mode).

**Thread safety:** A FreeRTOS mutex (`_tftMutex`) protects all SPI transactions. Both Core 1 (main loop) and Core 0 (speech task's `tftUpdateSpeechStatus`) acquire this mutex before any `tft.*` call.

```cpp
#define TFT_LOCK()   xSemaphoreTake(_tftMutex, portMAX_DELAY)
#define TFT_UNLOCK() xSemaphoreGive(_tftMutex)
```

**Dashboard layout (Y positions, portrait 240×320):**

| Y Position | Section | Content |
|---|---|---|
| 5 | Time | "HH:MM:SS" (right-aligned at x=140) |
| 20 | MPU label | "MPU:" (cyan) |
| 45 | MPU value | "X:__ Y:__ Z:__" (size 1) |
| 70 | Heart label | "Heart:" (red) |
| 75 | Heart value | "__ bpm" or "-- bpm" (size 2) |
| 105 | Heart status | "Finger Detected" or "No Finger" (size 1) |
| 130 | Sleep label | "Sleep:" (yellow) |
| 150 | Sleep value | State name, color-coded (size 2) |
| 180 | Stress label | "Mind:" (green) |
| 205 | Stress value | Level or "Please wear / finger band!" (size 2) |
| 230 | Stress quote | Random motivational quote (only for High stress, size 1) |
| 255 | Emergency | "!! EMERGENCY !!" (red, only when active) |
| 270 | Voice | "Voice: ..." (last transcript, 30 chars max, size 1) |
| 290 | IP address | "IP:xxx.xxx.xxx.xxx" (size 2) |

**Change detection:** Every `tftUpdate*` function stores the previous value in a `static` variable and returns immediately if the new value matches. This eliminates flicker from unnecessary redraws.

**"Please wear finger band" handling:** The string is 23 characters. At `textSize(2)` each character is 12 pixels wide = 276 pixels, which overflows the 240-pixel display width. Solution: split into two lines:
```
Line 1 (Y=205): "Please wear"
Line 2 (Y=227): "finger band!"
```
The fill rect clears a full 240×60 pixel area to cover both lines plus the quote area.

---

## 11. Network Modules

### 11.1 WiFi Manager

**Files:** `src/network/wifi_manager.h`, `src/network/wifi_manager.cpp`

Blocking STA-mode WiFi connection:
```cpp
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
// Poll every 500ms, up to WIFI_RETRY_MAX (30) times = 15 seconds max
```

Returns `true` on success, `false` on failure. The system continues running without WiFi — speech recognition and dashboard are simply unavailable.

### 11.2 OpenAI Whisper API

**Files:** `src/network/openai_api.h`, `src/network/openai_api.cpp`

**`openaiTranscribe(wavData, wavSize)` — full HTTP flow:**

1. Create `WiFiClientSecure` with `setInsecure()` (skip TLS certificate verification — necessary because ESP32 lacks a full certificate store).
2. Connect to `api.openai.com:443`.
3. Build multipart/form-data body:
   ```
   ----ESP32Boundary
   Content-Disposition: form-data; name="model"
   
   whisper-1
   ----ESP32Boundary
   Content-Disposition: form-data; name="file"; filename="audio.wav"
   Content-Type: audio/wav
   
   [WAV BINARY DATA]
   ----ESP32Boundary--
   ```
4. Send HTTP headers: `POST /v1/audio/transcriptions`, `Authorization: Bearer <key>`, `Content-Length`.
5. Upload WAV data in 1024-byte chunks, calling `esp_task_wdt_reset()` between chunks to prevent watchdog timeout.
6. Read response with 10-second timeout. `esp_task_wdt_reset()` and `vTaskDelay(1)` during read loop.
7. Strip HTTP headers (find `\r\n\r\n`), locate JSON brace.
8. Parse JSON with ArduinoJson: extract `doc["text"]`.
9. Return transcript string (empty on failure).

**Model used:** `whisper-1` (OpenAI's Whisper V2 Large model via API).

### 11.3 Web Server & Dashboard

**Files:** `src/network/web_server.h`, `src/network/web_server.cpp`

**Server:** `ESPAsyncWebServer` on port 80.

**Routes:**

| Method | Path | Handler |
|---|---|---|
| GET | `/` | Serve embedded HTML dashboard (PROGMEM) |
| GET | `/api/data` | JSON: all sensor values + `camOpen` flag |
| GET | `/api/logs` | JSON: ring buffer log entries |
| GET | `/api/logs/clear` | Clear log ring buffer |
| GET | `/api/breathe` | Trigger breathing LED callback |
| GET | `/api/alarm?state=on\|off` | Start/stop speaker alarm |
| GET | `/api/vibrate` | Trigger vibration pulse callback |
| GET | `/api/emergency/clear` | Clear emergency state callback |

**`DashboardState` struct (shared between main.cpp and web server):**

```cpp
struct DashboardState {
    float  gsrValue;          // conductance reading (µS-proportional)
    String stressLevel;       // "Low", "Moderate", "High", "Please wear finger band"
    int    heartBPM;          // rolling-average BPM (0 if no finger)
    bool   fingerPresent;     // true if IR > 10000
    String sleepQuality;      // "Deep Sleep", "Light Sleep", "Awake"
    bool   emergencyActive;   // true while emergency is active
    float  accelX, accelY, accelZ;  // raw MPU6050 accel (m/s²)
    float  temperature;       // MPU6050 die temperature (°C)
    String lastVoiceCommand;  // last OpenAI transcript
    bool   breathingActive;   // true while LED pattern is running
    bool   cameraOpen;        // one-shot: signal dashboard to open camera
};
```

The `cameraOpen` field is a **one-shot flag**: when the awake nudge fires, main.cpp sets it to `true`. The next time the dashboard polls `/api/data`, the server includes `"camOpen": true` and immediately resets it to `false`:

```cpp
doc["camOpen"] = dashState->cameraOpen;
dashState->cameraOpen = false;
```

**Callback architecture:** The web server does not directly call actuator functions. Instead, `webServerSetCallbacks()` registers 5 function pointers:
```cpp
void (*breatheCallback)()        // → ledBreathingStart(5)
void (*alarmOnCallback)()        // → speakerAlarmStart()
void (*alarmOffCallback)()       // → speakerAlarmStop()
void (*vibrateCallback)()        // → vibrationPulse(800)
void (*clearEmergencyCallback)() // → stop alarm, clear flag, update TFT + dashState
```

This keeps the web server module decoupled from actuator code.

**Dashboard HTML (embedded in PROGMEM):**

A single-page application with:
- 9 card grid (Heart Rate, GSR/Stress, Sleep, Motion, Emergency, Voice, Breathing LED, Speaker Alarm, Vibration)
- Emergency banner (red pulsing animation, hidden when `emergency === false`)
- Camera feed section (hidden by default, opens via `openCamera(seconds)` JS)
- Live logs panel with level filtering, pause, clear, and auto-scroll
- 1-second polling interval via `setInterval(update, 1000)`

**Key JavaScript functions:**
- `update()` — fetches `/api/data`, updates all card values, shows/hides emergency banner and dismiss button, triggers `openCamera(20)` if `d.camOpen && !camTimer`.
- `openCamera(seconds)` — sets `img.src` to `http://<hostname>:81/stream`, shows camera section, starts countdown timer, auto-hides after timeout.
- `triggerVibrate()` — calls `/api/vibrate` then `openCamera(20)`.
- `clearEmergency()` — calls `/api/emergency/clear` then `update()`.
- `refreshLogs(force)` — fetches `/api/logs`, formats with ANSI-like colors, respects filter and pause state.

### 11.4 Centralized Logger

**Files:** `src/network/logger.h`, `src/network/logger.cpp`

A ring-buffer logging system that writes to both Serial and RAM.

**Ring buffer:** 80 entries (`LOG_BUFFER_ENTRIES`), each containing:
- `timestamp` (millis())
- `level` (DEBUG/INFO/WARN/ERROR)
- `tag` (16 chars max, e.g., "HR", "GSR", "MAIN")
- `msg` (120 chars max)

**`logWrite(level, tag, fmt, ...)`:**
1. Check if `level >= minLevel` (filter).
2. Format message via `vsnprintf`.
3. Write to ring buffer at `head`, advance head circularly.
4. Print to Serial with ANSI color codes:
   - DEBUG: cyan (`\033[36m`)
   - INFO: green (`\033[32m`)
   - WARN: yellow (`\033[33m`)
   - ERROR: red (`\033[31m`)

**`logGetJSON()`:** Walks the ring buffer in chronological order and builds a JSON array string. Tags and messages are escaped (quotes → single quotes, newlines → spaces). Called by the `/api/logs` endpoint.

**Macros:** `LOG_DEBUG(tag, fmt, ...)`, `LOG_INFO(...)`, `LOG_WARN(...)`, `LOG_ERROR(...)` — convenience wrappers that call `logWrite` with the appropriate level.

---

## 12. Main Loop — Complete Execution Flow

```
loop() [Core 1, runs every ~1ms]
│
├── SECTION A — Every iteration (no gating)
│   │
│   ├── heartRateUpdate()
│   │     └── Drain MAX30102 FIFO, detect beats, maintain BPM average
│   │
│   ├── mpuUpdate()
│   │     └── Read accel/gyro/temp, save previous values
│   │     └── if delta > 0.2: lastMovementTime = now
│   │
│   ├── RTC poll (every 950ms)
│   │     └── if second changed: rtcFormatTime → tftUpdateTime
│   │
│   ├── Button check
│   │     └── Rising edge → toggle emergencyFlag
│   │     └── if ON: alarm start, TFT "EMERGENCY", dashState
│   │     └── if OFF: alarm stop, TFT clear, dashState
│   │
│   ├── No-movement nudge
│   │     └── if no movement > 40s: play buzzer sound every 5s
│   │
│   └── Actuator state machines
│         ├── ledBreathingUpdate()    — advance PWM one step
│         ├── speakerAlarmUpdate()    — alternate 800/1200 Hz
│         └── speakerBuzzerUpdate()   — stream 256 PCM samples
│
├── SECTION B — 1-second tick (gated by lastDisplayTick)
│   │
│   ├── Heart Rate display
│   │     ├── tftUpdateHeartRate(bpm, finger)
│   │     └── if abnormal HR: start breathing LED
│   │
│   ├── MPU display + sleep detector
│   │     ├── tftUpdateMPU(x, y, z)
│   │     ├── Compute dx, dy, dz, activeAxes
│   │     ├── sleepDetectorFeed(dx, dy, dz, activeAxes)
│   │     ├── tftUpdateSleep(quality)
│   │     └── Awake nudge logic (see §14)
│   │
│   ├── GSR stress
│   │     ├── gsrReadConductance()
│   │     ├── gsrGetStressLevel()
│   │     └── tftUpdateStress(level, value)
│   │
│   └── Hourly vibration
│         └── if interval elapsed AND not sleeping: vibrationPulse(1000)
│
└── yield()  — let WiFi/BT stack run
```

---

## 13. Emergency System

The emergency system has **three input sources** and **four output effects**.

**Inputs:**
1. **Physical button (GPIO41)** — toggle behavior. First press activates, second press deactivates. Detected as rising-edge in Section A.
2. **Voice command** — keywords "help", "help me", "emergency", "panic" detected by Core 0 speech task. Activates only (does not toggle). Cleared by "stop" or "cancel".
3. **Dashboard dismiss button** — sends `GET /api/emergency/clear`. Only visible when emergency is active.

**Outputs when active:**
1. **Speaker alarm** — two-tone siren (800/1200 Hz, alternating every 400ms)
2. **TFT display** — "!! EMERGENCY !!" in red at Y=255
3. **Dashboard banner** — red pulsing "⚠ EMERGENCY ALERT" bar at top
4. **Dashboard state** — `emergencyActive = true`, transmitted via `/api/data` JSON

**Clear (any of these):**
- Second button press
- Voice command "stop" / "cancel"
- Dashboard "✕ Dismiss" button → `/api/emergency/clear` → `onClearEmergency()`:
  ```cpp
  emergencyFlag = false;
  speakerAlarmStop();
  tftUpdateEmergency(false);
  dashState.emergencyActive = false;
  ```

---

## 14. Awake Nudge System

When the sleep detector reports "Awake" continuously, the system periodically nudges the parent by vibrating the motor and opening the camera feed on the dashboard.

**Timing variables:**
```cpp
static unsigned long awakeStartTime = 0;   // when "Awake" state began
static unsigned long lastAwakeNudge = 0;   // last nudge timestamp
```

**Logic (inside 1-second tick, after `sleepDetectorFeed`):**

```cpp
String sq = sleepDetectorGetQuality();

if (sq == "Awake") {
    if (now - lastAwakeNudge >= INTERVAL_AWAKE_NUDGE) {  // 60000ms = 1 min
        lastAwakeNudge = now;
        vibrationPulse(800);           // 800ms motor pulse
        dashState.cameraOpen = true;   // signal dashboard to open camera
    }
} else {
    // Child fell asleep — reset both timers
    awakeStartTime = now;
    lastAwakeNudge = now;
    dashState.cameraOpen = false;
}
```

**Dashboard side:** In the `update()` JS function:
```javascript
if (d.camOpen && !camTimer) { openCamera(20); }
```
This opens the camera for 20 seconds. The `camTimer` guard prevents re-opening if the camera is already showing.

**Effective behavior:**
- 0-60s awake: no action (first nudge fires at 60s)
- 60s awake: vibrate + camera for 20s
- 120s awake: vibrate + camera again
- 180s awake: vibrate + camera again
- ...continues every 60s while "Awake"
- As soon as sleep detector transitions to "Light Sleep" or "Deep Sleep": both timers reset, cameraOpen cleared.

---

## 15. Pin Mapping Reference

| GPIO | Function | Module | Direction |
|---|---|---|---|
| 0 | I2S LRC (Speaker) | MAX98357A | Output |
| 1 | TFT SCK (Software SPI) | ILI9341 | Output |
| 2 | TFT MOSI (Software SPI) | ILI9341 | Output |
| 3 | GSR ADC | GSR Sensor | Analog In |
| 4 | Camera SIOD (SCCB SDA) | OV2640 | Bidir |
| 5 | Camera SIOC (SCCB SCL) | OV2640 | Output |
| 6 | Camera VSYNC | OV2640 | Input |
| 7 | Camera HREF | OV2640 | Input |
| 8 | Camera D2 | OV2640 | Input |
| 9 | Camera D1 | OV2640 | Input |
| 10 | Camera D3 | OV2640 | Input |
| 11 | Camera D0 | OV2640 | Input |
| 12 | Camera D4 | OV2640 | Input |
| 13 | Camera PCLK | OV2640 | Input |
| 14 | I2S DIN (Speaker) | MAX98357A | Output |
| 15 | Camera XCLK | OV2640 | Output |
| 16 | Camera D7 | OV2640 | Input |
| 17 | Camera D6 | OV2640 | Input |
| 18 | Camera D5 | OV2640 | Input |
| 19 | I2S SD (Mic data) | INMP441 | Input |
| 20 | LED PWM | 4× LEDs + PNP | Output |
| 21 | I2S BCLK (Speaker) | MAX98357A | Output |
| 38 | I2S WS (Mic clock) | INMP441 | Output |
| 39 | TFT CS | ILI9341 | Output |
| 40 | TFT RST | ILI9341 | Output |
| 41 | Emergency Button | Tactile switch | Input (pull-up) |
| 42 | TFT DC | ILI9341 | Output |
| 45 | I2S SCK (Mic bit clock) | INMP441 | Output |
| 46 | Vibration motor | DC motor | Output |
| 47 | I2C SDA (sensors) | MPU6050/MAX30102/DS3231 | Bidir |
| 48 | I2C SCL (sensors) | MPU6050/MAX30102/DS3231 | Output |

---

## 16. Timing Constants Reference

Defined in `src/config.h`:

| Constant | Value | Purpose |
|---|---|---|
| `INTERVAL_GSR_CHECK` | 1000 ms | GSR reading interval (used as reference; actual rate is tied to 1s tick) |
| `INTERVAL_MPU_DISPLAY` | 1000 ms | MPU display update interval |
| `INTERVAL_SPEECH_RECOG` | 15000 ms | Time between speech recording cycles (Core 0) |
| `INTERVAL_VIBRATION_HOUR` | 60000 ms | Periodic vibration reminder (currently 1 min for testing; production: 3600000 ms = 1 hour) |
| `INTERVAL_AWAKE_NUDGE` | 60000 ms | Vibrate + camera every N ms while awake |
| `INTERVAL_SLEEP_WINDOW` | 30000 ms | Sleep evaluation window (30 samples × 1s/sample) |
| `INTERVAL_DASHBOARD_POLL` | 2000 ms | Reference value; JS-side actually polls every 1000 ms |
| `INTERVAL_DISPLAY_UPDATE` | 1000 ms | RTC display update interval |

---

## 17. Threshold Constants Reference

Defined in `src/config.h`:

| Constant | Value | Usage |
|---|---|---|
| `MOVEMENT_THRESHOLD` | 0.2 m/s² | Any-axis delta to reset no-movement timer |
| `SLEEP_MOVEMENT_DEEP` | 0.3 m/s² | Legacy (not used by current ring-buffer sleep detector) |
| `SLEEP_MOVEMENT_LIGHT` | 0.8 m/s² | Legacy (not used by current ring-buffer sleep detector) |
| `HR_ABNORMAL_HIGH` | 120 BPM | Above this: trigger breathing LED |
| `HR_ABNORMAL_LOW` | 50 BPM | Below this: trigger breathing LED |
| `NO_MOVEMENT_EMERGENCY_MS` | 40000 ms | No movement for 40s → nudge buzzer |
| `GSR_HIGH_THRESHOLD` | 24000.0 | GSR > 24000 → "High" stress |
| `GSR_MODERATE_LOW` | 14000.0 | GSR ≥ 14000 → "Moderate" stress |
| `GSR_LOW_THRESHOLD` | 7900.0 | GSR ≥ 7900 → "Low"; below → "Please wear finger band" |

**Sleep detector thresholds (hardcoded in `sleep_detector.cpp`, not in config.h):**

| Condition | Classification |
|---|---|
| `avgDelta > 1.5` | Awake |
| `avgDelta > 0.10` | Light Sleep |
| `avgDelta ≤ 0.10` | Deep Sleep |

**MPU active-axes threshold:** 0.3 m/s² (hardcoded in `main.cpp` as the argument to `mpuGetActiveAxes(0.3f)`).

---

## 18. File Structure

```
src/
├── config.h                          Central configuration (pins, WiFi, thresholds, timing)
├── main.cpp                          Main orchestrator (setup, loop, speech task, callbacks)
│
├── sensors/
│   ├── heart_rate.h / .cpp           MAX30102 — beat detection, 4-sample rolling BPM
│   ├── mpu.h / .cpp                  MPU6050 — accel/gyro read, delta computation
│   ├── gsr.h / .cpp                  GSR — 10-sample ADC average, stress classification
│   ├── rtc_clock.h / .cpp            DS3231 — time read, format, compile-time set
│   └── microphone.h / .cpp           INMP441 — I2S recording, WAV header generation
│
├── actuators/
│   ├── led_breathing.h / .cpp        PWM breathing pattern (non-blocking state machine)
│   ├── vibration.h / .cpp            DC motor pulse (blocking)
│   ├── speaker.h / .cpp              I2S tone gen, alarm siren, PCM buzzer
│   └── buzzer_pcm.h                  2765-sample PCM waveform array (flash)
│
├── display/
│   └── tft_display.h / .cpp          ILI9341 — layout, change-guarded updates, mutex
│
├── camera/
│   └── camera.h / .cpp               OV2640 — init, MJPEG stream server on port 81
│
├── network/
│   ├── wifi_manager.h / .cpp         STA-mode WiFi connection with retry
│   ├── openai_api.h / .cpp           HTTPS multipart POST to Whisper API
│   ├── web_server.h / .cpp           AsyncWebServer, embedded dashboard HTML, JSON API
│   └── logger.h / .cpp               Ring-buffer logger (Serial + RAM + JSON endpoint)
│
└── logic/
    ├── sleep_detector.h / .cpp       30-sample ring buffer, delta-based classification
    └── command_handler.h / .cpp      Keyword matching for voice commands
```

---

*Document generated from codebase as of March 3, 2026. All descriptions are derived directly from the source files.*
