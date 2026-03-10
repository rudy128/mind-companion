# 🧠 M.I.N.D. Companion

**Mental Intelligence & Neurological Device Companion**  
An ESP32-S3 wearable/bedside device for real-time mental wellness monitoring — heart rate, stress, sleep quality, motion detection, voice commands, and emergency alerts. Sensor data streams in real time to a **Next.js dashboard** via **MQTT over WebSockets**.

---

## Table of Contents

- [Overview](#overview)
- [Hardware](#hardware)
- [Pin Map](#pin-map)
- [Software Architecture](#software-architecture)
- [Project Structure](#project-structure)
- [Features](#features)
- [MQTT Protocol](#mqtt-protocol)
- [Next.js Dashboard](#nextjs-dashboard)
- [Getting Started](#getting-started)
- [Configuration](#configuration)
- [Libraries Used](#libraries-used)

---

## Overview

M.I.N.D. Companion runs on a **Freenove ESP32-S3-WROOM-1-N8R8** (8 MB Flash + 8 MB PSRAM, built-in OV2640 camera). It continuously monitors the user's physiological signals, renders them on a local TFT display, streams them to a **Next.js dashboard via MQTT**, and takes automated actions (breathing LEDs, vibration reminders, speaker alarms, audio quotes) when thresholds are crossed.

A local **Mosquitto** broker bridges the ESP32 (MQTT on port 1883) and the browser dashboard (MQTT-over-WebSocket on port 9001). Voice commands are transcribed via **OpenAI Whisper** and handled by a built-in command parser (currently disabled while sensor + MQTT stability is being hardened).

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

| Core | Role | Details |
|------|------|---------|
| **Core 1** | `loop()` — sensors, TFT display, actuators | Runs every ~1 ms. Heart rate FIFO drain (50 ms), MPU read (50 ms), RTC poll (950 ms), button poll, non-blocking speaker/buzzer/vibration/LED state machines. 1-second tick updates TFT and writes shared `dashState`. |
| **Core 0** | MQTT FreeRTOS task (priority 2) | 50 ms tick: `_mqtt.loop()` for keepalive pings + incoming commands. 1 Hz: publishes `dashState` JSON to `mind/data`. Reconnects automatically on drop. |
| **Core 0** | Speech task *(currently disabled)* | Records 2 s audio → OpenAI Whisper → command parser. Disabled while MQTT stability is being hardened. |
| **Core 0** | WiFi internals + camera HTTP | ESP-IDF WiFi task + `esp_http_server` for MJPEG stream on port 81. |

**Key design decisions:**

- **MQTT on Core 0**: PubSubClient socket timeouts blocked the sensor loop when running on Core 1. Moving MQTT to a dedicated FreeRTOS task on Core 0 eliminated all keepalive timeout issues. The task runs at 200 ticks / 10 s with zero disconnects.
- **Cross-core shared struct**: Core 1 writes `MqttDashState` fields; Core 0 reads them for publishing. Primitive fields are marked `volatile` to ensure cross-core memory visibility. No mutex needed for telemetry (individual fields are atomic-width).
- **TFT mutex**: All `tftUpdate*()` functions acquire a FreeRTOS mutex (`portMAX_DELAY`) to prevent SPI bus collisions between cores.
- **Non-blocking actuators**: Speaker alarm, nudge buzzer, vibration, and breathing LED all use state-machine patterns — they write one I2S chunk per `loop()` call instead of blocking.
- **I2S port isolation**: Microphone uses I2S #0 (RX), speaker uses I2S #1 (TX). The ESP32-audioI2S `Audio` object is lazy-initialized (`new Audio(I2S_NUM_1)`) to prevent static-init I2S conflicts.
- **Heart rate config**: MAX30102 at 100 Hz / 4× hardware averaging / 411 µs pulse width / LED brightness 0x2F. `particleSensor.check()` called every 50 ms, FIFO drained with `while(available())`, 4-beat rolling BPM average.

---

## Project Structure

```
src/
├── config.h                    ← All pin defs, credentials, thresholds, MQTT topics
├── main.cpp                    ← Setup, dual-core orchestrator, loop (Core 1)
│
├── sensors/
│   ├── heart_rate.cpp/h        ← MAX30102: 100Hz/4×avg, FIFO drain, beat detection
│   ├── mpu.cpp/h               ← MPU6050: accelerometer + per-axis deltas
│   ├── gsr.cpp/h               ← GSR: 10-sample ADC average → resistance (kΩ)
│   ├── rtc_clock.cpp/h         ← DS3231: current time string
│   └── microphone.cpp/h        ← INMP441: I2S #0 capture (install-on-demand driver)
│
├── actuators/
│   ├── audio_quotes.cpp/h      ← MP3 quote playback via ESP32-audioI2S (lazy init)
│   ├── led_breathing.cpp/h     ← PWM sinusoidal breathing pattern
│   ├── vibration.cpp/h         ← Vibration motor pulse (non-blocking)
│   ├── speaker.cpp/h           ← I2S #1 tone / alarm / buzzer (non-blocking state machines)
│   ├── buzzer.cpp/h            ← Passive buzzer
│   └── buzzer_pcm.h            ← Pre-computed PCM waveform for nudge buzzer
│
├── display/
│   └── tft_display.cpp/h       ← ILI9341 layout: HR, sleep, stress, time, MPU, emergency
│
├── camera/
│   └── camera.cpp/h            ← OV2640 init + MJPEG stream server (port 81)
│
├── network/
│   ├── wifi_manager.cpp/h      ← Wi-Fi connect with retry
│   ├── mqtt_client.cpp/h       ← MQTT client — Core 0 FreeRTOS task (PubSubClient)
│   ├── openai_api.cpp/h        ← HTTPS POST to Whisper transcription endpoint
│   ├── logger.cpp/h            ← Serial-only logger (level-filtered, colour-coded)
│   └── web_server.cpp/h        ← (Legacy) AsyncWebServer — replaced by MQTT
│
├── logic/
│   ├── sleep_detector.cpp/h    ← 30-sample ring buffer → Deep/Light/Restless/Awake
│   └── command_handler.cpp/h   ← Voice command parsing → actuator callbacks
│
dashboard/                       ← Next.js dashboard (separate app)
├── app/                         ← Next.js app router pages
├── components/
│   └── dashboard.tsx            ← Main dashboard UI component
├── lib/
│   ├── types.ts                 ← SensorData interface
│   └── use-mqtt.ts              ← React hook: MQTT-over-WebSocket subscription
├── package.json                 ← pnpm, mqtt@5.10.4, next, react, tailwind
└── ...
```

---

## Features

### 🫀 Heart Rate Monitoring
- MAX30102 configured at **100 Hz**, 4× hardware averaging, 411 µs pulse width, 4096 ADC range
- LED brightness 0x2F for reliable finger detection
- `particleSensor.check()` called every 50 ms, FIFO drained with `while(available())`
- 4-beat rolling average for stable BPM
- Graceful finger-on / finger-off detection with automatic state reset
- Published to dashboard via MQTT at 1 Hz

### 😰 Stress Monitoring (GSR)
- Galvanic skin response read from GPIO 3 (10-sample average)
- Classified as **Low / Moderate / High / Please wear finger straps**
- High stress triggers motivational quote on TFT + MP3 audio playback from LittleFS

### 😴 Sleep Quality Detection
- MPU6050 accelerometer feeds a 30-sample ring buffer (1 sample/second)
- Per-axis deltas + active-axis count fed to the sleep detector
- Classifies: **Deep Sleep / Light Sleep / Restless / Awake**
- Awake ≥ 1 minute triggers vibration pulse + camera flag for dashboard

### 🔊 Audio Quote Playback
- Calming MP3 quotes stored on LittleFS (`/q1.mp3`, etc.)
- Played via ESP32-audioI2S library on I2S #1 (lazy-initialized)
- Triggered by high GSR stress (max once per 30 seconds)
- Quote text shown on TFT display simultaneously

### 🗣 Voice Commands (OpenAI Whisper) *(currently disabled)*
- Records 2 seconds of audio from INMP441 (I2S #0) every 15 seconds
- Sends raw PCM to OpenAI Whisper API via HTTPS
- Parsed commands: `breathe`, `alarm on/off`, `vibrate`, `stop`
- Disabled while MQTT + sensor stability is being hardened

### 📷 Live Camera Stream
- OV2640 MJPEG stream served via `esp_http_server` at `http://<IP>:81/stream`
- Dashboard auto-detects ESP32 IP from MQTT telemetry

### ⚠ Emergency Alert
- Physical button on GPIO 41 (active LOW, internal pull-up) — **toggle** behaviour
- Press once → activates alarm + TFT banner + MQTT alert (`mind/alert`)
- Press again → clears emergency
- Can also be cleared via MQTT command from dashboard (`{"cmd":"clear_emergency"}`)

### 🔔 No-Movement Nudge
- If MPU6050 detects no movement for **40 seconds**, a buzzer nudge beep fires every 5 seconds
- Uses a pre-computed PCM waveform (`buzzer_pcm.h`) played non-blocking via I2S

### 💡 Breathing LED
- Smooth sinusoidal PWM pattern on GPIO 20
- Triggered by MQTT command `{"cmd":"breathe"}` from dashboard

### 📳 Vibration Motor
- Non-blocking pulse pattern on GPIO 46
- Triggered by awake nudge (1 min) or MQTT command `{"cmd":"vibrate"}`

---

## MQTT Protocol

Communication between the ESP32 and the dashboard uses MQTT via a local **Mosquitto** broker.

### Broker Setup

Mosquitto runs on your laptop with a dual-listener configuration:

| Listener | Port | Protocol | Client |
|----------|------|----------|--------|
| 1 | **1883** | MQTT (TCP) | ESP32 PubSubClient |
| 2 | **9001** | WebSocket | Next.js dashboard (browser) |

Example `mosquitto.conf`:
```
listener 1883
protocol mqtt

listener 9001
protocol websockets

allow_anonymous true
```

### Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `mind/data` | ESP32 → Dashboard | Sensor telemetry JSON (1 Hz) |
| `mind/logs` | ESP32 → Dashboard | Log entries |
| `mind/alert` | ESP32 → Dashboard | Emergency flag changes (retained) |
| `mind/cmd` | Dashboard → ESP32 | Commands: `{"cmd":"breathe"}`, `{"cmd":"alarm_on"}`, etc. |

### `mind/data` payload example

```json
{
  "bpm": 67,
  "finger": true,
  "gsr": 3200.5,
  "stress": "Low",
  "sleep": "Awake",
  "emergency": false,
  "ax": "0.02",
  "ay": "-0.01",
  "az": "0.98",
  "temp": "26.3",
  "voice": "",
  "breathing": false,
  "camOpen": false,
  "ip": "172.20.10.8",
  "heap": 8589459,
  "uptime": 300
}
```

### `mind/cmd` commands

| Command | Action |
|---------|--------|
| `breathe` | Start breathing LED pattern |
| `alarm_on` | Start speaker alarm |
| `alarm_off` | Stop speaker alarm |
| `vibrate` | Pulse vibration motor |
| `clear_emergency` | Clear emergency state |

---

## Next.js Dashboard

The dashboard is a standalone **Next.js** app in the `dashboard/` folder. It connects to the Mosquitto broker via **MQTT-over-WebSocket** (port 9001) and renders real-time sensor data.

### Running the Dashboard

```bash
cd dashboard
pnpm install
pnpm dev
# → http://localhost:3000
```

### Key Files

| File | Purpose |
|------|---------|
| `lib/use-mqtt.ts` | React hook — connects to `ws://localhost:9001`, subscribes to `mind/data` and `mind/alert` |
| `lib/types.ts` | `SensorData` TypeScript interface |
| `components/dashboard.tsx` | Main dashboard UI — auto-detects ESP32 IP from telemetry |

### Dashboard Features

| Section | Description |
|---------|-------------|
| Heart Rate | Live BPM + finger detection status |
| Stress Level | GSR reading with Low/Moderate/High badge |
| Sleep Quality | Current classification |
| Motion | X/Y/Z accelerometer values |
| Emergency | Real-time emergency status + clear button |
| Camera | Live MJPEG stream (auto-detected ESP32 IP) |
| Controls | Breathe, alarm, vibrate commands sent via `mind/cmd` |

---

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- ESP32-S3 board support (`espressif32` platform)
- [Mosquitto](https://mosquitto.org/) MQTT broker installed locally
- [Node.js](https://nodejs.org/) + [pnpm](https://pnpm.io/) for the dashboard
- Wi-Fi network that both your laptop and ESP32 can join
- *(Optional)* OpenAI API key for voice commands

### Build & Flash

```bash
cd "Merged Code"

# Build firmware
pio run

# Upload firmware
pio run --target upload

# Upload LittleFS data (MP3 quotes, etc.)
pio run --target uploadfs

# Monitor serial output
pio device monitor --baud 115200
```

### Start the Broker

```bash
# Windows (installed as service, or run manually):
mosquitto -v -c mosquitto_mind.conf

# macOS / Linux:
mosquitto -v -c mosquitto_mind.conf
```

### Start the Dashboard

```bash
cd dashboard
pnpm install
pnpm dev
# → http://localhost:3000
```

### First Boot

1. ESP32 connects to Wi-Fi (SSID configured in `config.h`)
2. Serial monitor prints: `[WiFi] Connected! IP: 172.20.10.x`
3. MQTT connects: `[MQTT] Connected to broker`
4. MQTT task starts on Core 0: `[MQTT] Task started on Core 0`
5. Open `http://localhost:3000` — dashboard auto-connects via WebSocket
6. Place finger on MAX30102 sensor — BPM appears within ~2–3 seconds
7. Camera stream available at `http://<ESP32_IP>:81/stream`

---

## Configuration

All configuration lives in `src/config.h`. Key settings:

```cpp
// Wi-Fi
#define WIFI_SSID          "YourNetwork"
#define WIFI_PASSWORD      "YourPassword"

// MQTT Broker (your laptop's LAN IP)
#define MQTT_SERVER        "172.20.10.3"
#define MQTT_PORT          1883
#define MQTT_CLIENT_ID     "mind-companion"

// MQTT Topics
#define MQTT_TOPIC_DATA    "mind/data"     // sensor telemetry (1 Hz)
#define MQTT_TOPIC_LOGS    "mind/logs"     // log messages
#define MQTT_TOPIC_ALERT   "mind/alert"    // emergency events
#define MQTT_TOPIC_CMD     "mind/cmd"      // commands from dashboard

// OpenAI (optional — speech task disabled by default)
#define OPENAI_API_KEY     "sk-..."

// Timing
#define NO_MOVEMENT_EMERGENCY_MS 40000     // 40 s → nudge buzzer
#define INTERVAL_AWAKE_NUDGE     60000UL   // vibrate + camera after 1 min awake
#define INTERVAL_VIBRATION_HOUR  60000UL   // vibration reminder interval

// Heart Rate Thresholds
#define HR_ABNORMAL_HIGH   120   // BPM
#define HR_ABNORMAL_LOW     50   // BPM

// GSR Thresholds (resistance in kΩ — lower = more stress)
#define GSR_WEAR_THRESHOLD      8000.0f    // above = not wearing straps
#define GSR_LOW_THRESHOLD       4000.0f
#define GSR_MODERATE_THRESHOLD  2000.0f
```

> **Security note**: `OPENAI_API_KEY`, Wi-Fi credentials, and `MQTT_SERVER` are hardcoded. Before sharing or committing, move them to a `secrets.h` file listed in `.gitignore`.

---

## Libraries Used

### ESP32 Firmware

| Library | Version | Purpose |
|---|---|---|
| `espressif32` platform | latest | ESP32-S3 Arduino support |
| ArduinoJson | ^7.4.2 | JSON serialisation for MQTT payloads |
| PubSubClient | ^2.8 | MQTT client (TCP) |
| SparkFun MAX3010x | ^1.1.2 | MAX30102 heart rate sensor |
| Adafruit MPU6050 | ^2.2.9 | MPU6050 accelerometer driver |
| Adafruit ILI9341 | ^1.6.3 | TFT display driver |
| RTClib | ^2.1.4 | DS3231 real-time clock |
| esp32-camera | ^2.0.4 | OV2640 camera driver |
| ESP32-audioI2S | ^2.3.0 | MP3 playback from LittleFS |

### Next.js Dashboard

| Package | Version | Purpose |
|---|---|---|
| next | latest | React framework |
| mqtt | ^5.10.4 | MQTT-over-WebSocket client (browser) |
| react | latest | UI library |
| tailwindcss | latest | Utility-first CSS |

---

## License

This project is for educational and personal use. OpenAI API usage is subject to [OpenAI's Terms of Service](https://openai.com/policies/terms-of-use).
