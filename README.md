# 🧠 M.I.N.D. Companion

**Mental Intelligence & Neurological Device Companion**  
An ESP32-S3 wearable/bedside device for real-time mental wellness monitoring — heart rate, stress, sleep quality, motion detection, voice commands, and emergency alerts.

---

## Table of Contents

- [Overview](#overview)
- [Hardware](#hardware)
- [Pin Map](#pin-map)
- [Software Architecture](#software-architecture)
- [Project Structure](#project-structure)
- [Features](#features)
- [Web Dashboard](#web-dashboard)
- [Getting Started](#getting-started)
- [Configuration](#configuration)
- [API Reference](#api-reference)
- [Libraries Used](#libraries-used)

---

## Overview

M.I.N.D. Companion runs on a **Freenove ESP32-S3-WROOM-1-N8R8** (8 MB Flash + 8 MB PSRAM, built-in OV2640 camera). It continuously monitors the user's physiological signals, renders them on a local TFT display, streams them to a Wi-Fi web dashboard, and takes automated actions (breathing LEDs, vibration reminders, speaker alarms) when thresholds are crossed.

Voice commands are transcribed via **OpenAI Whisper** and handled by a built-in command parser, making the device conversational.

---

## Hardware

| Component | Part | Interface |
|---|---|---|
| Microcontroller | Freenove ESP32-S3-WROOM-1-N8R8 | — |
| Display | ILI9341 2.8" TFT (240×320) | Software SPI |
| Heart Rate | MAX30102 Pulse Oximeter | I2C (0x57) |
| IMU | MPU6050 6-axis (AD0=HIGH → 0x69) | I2C (0x69) |
| RTC | DS3231 Real-Time Clock | I2C (0x68) |
| GSR Sensor | Galvanic Skin Response (analog) | ADC GPIO 3 |
| Microphone | INMP441 MEMS (I2S) | I2S #0 RX |
| Speaker | MAX98357A Class-D amplifier (I2S) | I2S #1 TX |
| Camera | OV2640 (built-in) | DVP/SCCB |
| LED Strip | Breathing LED via PNP transistor | PWM GPIO 20 |
| Vibration Motor | Coin motor | GPIO 46 |
| Emergency Button | Tactile switch (active LOW) | GPIO 41 |

---

## Pin Map

### I2C Bus (shared)
| Signal | GPIO |
|---|---|
| SDA | 47 |
| SCL | 48 |

### TFT Display (ILI9341 — Software SPI)
| Signal | GPIO |
|---|---|
| CS | 39 |
| RST | 40 |
| DC | 42 |
| MOSI | 2 |
| SCK | 1 |

### Microphone (INMP441 — I2S #0)
| Signal | GPIO |
|---|---|
| SCK | 45 |
| WS | 38 |
| SD | 19 |

### Speaker (MAX98357A — I2S #1)
| Signal | GPIO |
|---|---|
| BCLK | 21 |
| LRC | 0 |
| DIN | 14 |

### Camera (OV2640 — built-in)
| Signal | GPIO |
|---|---|
| XCLK | 15 |
| SIOD | 4 |
| SIOC | 5 |
| D0–D7 | 11, 9, 8, 10, 12, 18, 17, 16 |
| VSYNC | 6 |
| HREF | 7 |
| PCLK | 13 |

---

## Software Architecture

The firmware uses a **dual-core FreeRTOS** design:

```
┌──────────────────────────────────────┐  ┌─────────────────────────────────┐
│          Core 1 — loop()             │  │     Core 0 — speechTask()       │
│                                      │  │                                 │
│  Every iteration (~0.1 ms):          │  │  Every 15 seconds:              │
│    heartRateUpdate()  ← FIFO drain   │  │    micRecord() — 2s I2S capture │
│    mpuUpdate()                       │  │    openaiTranscribe() — Whisper  │
│    sleepDetectorFeed()               │  │    commandHandler parse          │
│    button poll (emergency)           │  │    tftUpdateSpeechStatus()       │
│    ledBreathingUpdate()              │  │                                 │
│    speakerAlarmUpdate()              │  │  Isolated so HTTP latency never  │
│                                      │  │  stalls sensors or display.      │
│  Every 1 second (lastDisplayTick):   │  └─────────────────────────────────┘
│    tftUpdateTime()                   │
│    tftUpdateHeartRate()              │
│    tftUpdateMPU()                    │
│    tftUpdateSleep()                  │
│    tftUpdateStress()                 │
│    dashState update (→ web API)      │
│    vibration hourly reminder check   │
└──────────────────────────────────────┘
```

**TFT display** is initialised once in `setup()`. All `tftUpdate*()` functions include internal change-guards — they only redraw when a value actually changes.

---

## Project Structure

```
src/
├── config.h                  ← All pin defs, credentials, thresholds, intervals
├── main.cpp                  ← Setup, dual-core orchestrator, loop
│
├── sensors/
│   ├── heart_rate.cpp/h      ← MAX30102: 400Hz, FIFO drain, beat detection
│   ├── mpu.cpp/h             ← MPU6050: accelerometer + sleep motion data
│   ├── gsr.cpp/h             ← GSR: averaged ADC → resistance (kΩ)
│   ├── rtc_clock.cpp/h       ← DS3231: current time string
│   └── microphone.cpp/h      ← INMP441: I2S capture into int16 buffer
│
├── actuators/
│   ├── led_breathing.cpp/h   ← PWM sinusoidal breathing pattern
│   ├── vibration.cpp/h       ← Vibration motor pulse
│   ├── speaker.cpp/h         ← I2S tone / alarm playback
│   └── buzzer.cpp/h          ← Passive buzzer
│
├── display/
│   └── tft_display.cpp/h     ← ILI9341 layout: HR, sleep, stress, time, speech
│
├── camera/
│   └── camera.cpp/h          ← OV2640 init + JPEG frame capture
│
├── network/
│   ├── wifi_manager.cpp/h    ← Wi-Fi connect with retry
│   ├── web_server.cpp/h      ← AsyncWebServer: dashboard + REST API
│   ├── openai_api.cpp/h      ← HTTPS POST to Whisper transcription endpoint
│   └── logger.cpp/h          ← Ring-buffer logger (80 entries), JSON export
│
└── logic/
    ├── sleep_detector.cpp/h  ← Movement window → Deep/Light/Restless/Awake
    └── command_handler.cpp/h ← Voice command parsing → actuator callbacks
```

---

## Features

### 🫀 Heart Rate Monitoring
- MAX30102 configured at **400 Hz**, 69 µs pulse width, 4096 ADC range
- FIFO fully drained every loop iteration — sub-100 ms beat detection
- 4-beat rolling average for stable BPM
- Graceful finger-on / finger-off detection with automatic reset

### 😰 Stress Monitoring (GSR)
- Galvanic skin response read from GPIO 3 (10-sample average)
- Classified as **Low / Moderate / High** stress
- Triggers motivational quote on TFT when stress is high

### 😴 Sleep Quality Detection
- MPU6050 accelerometer feeds a 30-second motion window
- Classifies: **Deep Sleep / Light Sleep / Restless / Awake**
- Automatically activates breathing LED if heart rate is abnormal (< 50 or > 120 BPM)

### 🗣 Voice Commands (OpenAI Whisper)
- Records 2 seconds of audio from INMP441 every 15 seconds
- Sends raw PCM to OpenAI Whisper API via HTTPS
- Parsed commands: `breathe`, `alarm on/off`, `vibrate`, and custom phrases

### 📷 Live Camera Stream
- OV2640 JPEG stream served at `/camera`
- Embedded in web dashboard as a live `<img>` feed

### ⚠ Emergency Alert
- Physical button on GPIO 41 (active LOW, internal pull-up)
- Triggers flashing banner on web dashboard + speaker alarm
- 8-hour no-movement detection also triggers speaker alarm

### 💡 Breathing LED
- Smooth sinusoidal PWM pattern on GPIO 20
- Activated by abnormal heart rate or explicit `breathe` voice command

### ⏱ Vibration Reminders
- Vibration pulse every **1 hour** as a wellness check-in reminder

---

## Web Dashboard

Accessible at `http://<ESP32_IP>/` once connected to Wi-Fi.

| Section | Description |
|---|---|
| Live Camera | MJPEG-compatible stream from OV2640 |
| Heart Rate | Current BPM with finger detection status |
| Stress Level | GSR reading with Low/Moderate/High badge |
| Sleep Quality | Current sleep classification |
| Accelerometer | X/Y/Z acceleration values |
| Time | Current RTC time |
| Emergency | Manual emergency trigger button |
| Controls | Breathing LED, alarm on/off |
| Logs Panel | Live colour-coded log stream with level filter |

The dashboard polls `/api/data` every **1 second**.

---

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- ESP32-S3 board support (`espressif32` platform)
- Wi-Fi network credentials (see [Configuration](#configuration))
- OpenAI API key with Whisper access

### Build & Flash

```bash
# Clone / open project in PlatformIO
cd "Merged Code"

# Build
pio run

# Upload firmware
pio run --target upload

# Upload LittleFS filesystem (dashboard.html if used)
pio run --target uploadfs

# Monitor serial output
pio device monitor --baud 115200
```

### First Boot

1. Device connects to Wi-Fi (configured in `config.h`)
2. Serial monitor prints the assigned IP address
3. Open `http://<IP>/` in a browser
4. Place finger on MAX30102 sensor — BPM appears within ~2 seconds

---

## Configuration

All configuration lives in `src/config.h`. Key settings:

```cpp
// Wi-Fi
#define WIFI_SSID          "YourNetwork"
#define WIFI_PASSWORD      "YourPassword"

// OpenAI
#define OPENAI_API_KEY     "sk-..."   // ⚠ Move to secrets file before committing

// Timing intervals (ms)
#define INTERVAL_GSR_CHECK       1000UL   // GSR read
#define INTERVAL_MPU_DISPLAY     1000UL   // MPU read
#define INTERVAL_SPEECH_RECOG    15000UL  // Voice command cycle
#define INTERVAL_VIBRATION_HOUR  3600000UL

// Thresholds
#define HR_ABNORMAL_HIGH   120   // BPM — triggers breathing LED
#define HR_ABNORMAL_LOW     50   // BPM — triggers breathing LED
#define GSR_HIGH_THRESHOLD 24000.0f
#define MOVEMENT_THRESHOLD  0.2f  // g
```

> **Security note**: `OPENAI_API_KEY` and Wi-Fi credentials are currently hardcoded. Before sharing or committing this project, move them to a `secrets.h` file that is listed in `.gitignore`.

---

## API Reference

All endpoints are served by the on-device AsyncWebServer at port 80.

| Method | Path | Description |
|---|---|---|
| `GET` | `/` | Full web dashboard (HTML) |
| `GET` | `/api/data` | JSON snapshot of all sensor values |
| `GET` | `/camera` | JPEG camera stream |
| `GET` | `/api/logs` | JSON array of last 80 log entries |
| `POST` | `/api/logs/clear` | Clear the log ring buffer |

### `/api/data` response example

```json
{
  "bpm": 72,
  "fingerDetected": true,
  "gsrResistance": 18500.0,
  "stressLevel": "Moderate",
  "sleepQuality": "Light Sleep",
  "accelX": 0.02,
  "accelY": -0.01,
  "accelZ": 0.98,
  "currentTime": "14:32:05",
  "emergency": false,
  "speechStatus": "Idle"
}
```

### `/api/logs` response example

```json
[
  {"ts": 12345, "level": "INFO",  "tag": "HR",   "msg": "Finger detected"},
  {"ts": 12346, "level": "DEBUG", "tag": "HR",   "msg": "Beat! delta=833ms raw_BPM=72"},
  {"ts": 12350, "level": "WARN",  "tag": "GSR",  "msg": "High stress detected"}
]
```

---

## Libraries Used

| Library | Version | Purpose |
|---|---|---|
| `espressif32` platform | latest | ESP32-S3 Arduino support |
| ArduinoJson | ^7.4.2 | JSON serialisation for API |
| Adafruit MPU6050 | ^2.2.9 | MPU6050 accelerometer driver |
| Adafruit ILI9341 | ^1.6.3 | TFT display driver |
| RTClib | ^2.1.4 | DS3231 real-time clock |
| SparkFun MAX3010x | ^1.1.2 | MAX30102 heart rate sensor |
| esp32-camera | ^2.0.4 | OV2640 camera driver |
| ESPAsyncWebServer | ^3.10.0 | Non-blocking web server |
| AsyncTCP | ^3.4.10 | Async TCP layer for ESPAsyncWebServer |

---

## License

This project is for educational and personal use. OpenAI API usage is subject to [OpenAI's Terms of Service](https://openai.com/policies/terms-of-use).
