// =============================================================
// OpenAI API — Whisper STT + TTS Implementation
// =============================================================
#include "openai_api.h"
#include "../config.h"
#include "../actuators/speaker.h"
#include "../network/logger.h"
#include "../network/wifi_manager.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <LittleFS.h>
#include <Audio.h>
#include <driver/i2s.h>   // for i2s_driver_uninstall / reinstall around Audio playback

#define TTS_WAV_PATH  "/tts_out.wav"


String openaiTranscribe(uint8_t* wavData, size_t wavSize) {
    WiFiClientSecure client;
    client.setInsecure();  // skip certificate verification on ESP32

    Serial.println("[AI] Connecting to api.openai.com...");
    if (!client.connect("api.openai.com", 443)) {
        Serial.println("[AI] Connection failed!");
        return "";
    }

    String boundary  = "----ESP32Boundary";
    String bodyStart = "--" + boundary + "\r\n"
                       "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
                       "whisper-1\r\n"
                       "--" + boundary + "\r\n"
                       "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
                       "Content-Type: audio/wav\r\n\r\n";
    String bodyEnd   = "\r\n--" + boundary + "--\r\n";

    size_t contentLength = bodyStart.length() + wavSize + bodyEnd.length();

    // HTTP headers
    client.print("POST /v1/audio/transcriptions HTTP/1.1\r\n");
    client.print("Host: api.openai.com\r\n");
    client.print("Authorization: Bearer " + String(OPENAI_API_KEY) + "\r\n");
    client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
    client.print("Content-Length: " + String(contentLength) + "\r\n\r\n");

    // Body
    client.print(bodyStart);
    size_t chunkSize = 1024;
    for (size_t i = 0; i < wavSize; i += chunkSize) {
        size_t len = min(chunkSize, wavSize - i);
        client.write(wavData + i, len);
        esp_task_wdt_reset();   // keep WDT happy during multi-second upload
        vTaskDelay(1);
    }
    client.print(bodyEnd);

    // Read response (skip HTTP headers, find JSON body)
    String response = "";
    unsigned long timeout = millis();
    bool headersEnded = false;
    while (client.connected() && millis() - timeout < 10000) {
        while (client.available()) {
            char c = client.read();
            response += c;
            timeout = millis();
        }
        // Yield to FreeRTOS scheduler so IDLE0 can run and feed the
        // task watchdog.  Without this the ~10s HTTP wait triggers WDT.
        esp_task_wdt_reset();
        vTaskDelay(1);
    }
    client.stop();

    // Extract JSON from response (after \r\n\r\n)
    int jsonStart = response.indexOf("\r\n\r\n");
    if (jsonStart < 0) jsonStart = 0; else jsonStart += 4;
    String jsonBody = response.substring(jsonStart);

    // In case of chunked encoding, find the JSON object
    int braceStart = jsonBody.indexOf('{');
    if (braceStart > 0) jsonBody = jsonBody.substring(braceStart);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonBody);
    if (err) {
        Serial.println("[AI] JSON parse error: " + String(err.c_str()));
        Serial.println("[AI] Raw: " + jsonBody.substring(0, 200));
        return "";
    }

    String text = doc["text"] | "";
    Serial.println("[AI] Transcription: " + text);
    return text;
}

// =============================================================
//  OpenAI TTS — request WAV from API, save to LittleFS,
//  play back with Audio library (exactly like the reference sketch)
// =============================================================
bool openaiSpeak(const char* text) {
    LOG_INFO("AI-TTS", "speak() → \"%s\"", text);

    // ── 0. Verify WiFi/DNS is healthy before touching HTTPClient ─
    if (!wifiEnsureConnected()) {
        LOG_ERROR("AI-TTS", "WiFi not ready — aborting TTS");
        return false;
    }

    // ── 1. Mount LittleFS ─────────────────────────────────
    if (!LittleFS.begin(false)) {
        LOG_WARN("AI-TTS", "LittleFS mount failed — formatting...");
        LittleFS.format();
        if (!LittleFS.begin(false)) {
            LOG_ERROR("AI-TTS", "LittleFS init failed");
            return false;
        }
    }

    // ── 2. POST to OpenAI TTS, response_format = wav ─────
    HTTPClient http;
    http.begin("https://api.openai.com/v1/audio/speech");
    http.addHeader("Content-Type",  "application/json");
    http.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);

    StaticJsonDocument<256> doc;
    doc["model"]  = "tts-1";
    doc["voice"]  = "alloy";
    doc["input"]  = text;
    doc["response_format"] = "wav";
    String body;
    serializeJson(doc, body);

    LOG_INFO("AI-TTS", "POST /v1/audio/speech ...");
    int code = http.POST(body);
    LOG_INFO("AI-TTS", "HTTP response: %d", code);

    if (code != 200) {
        LOG_ERROR("AI-TTS", "Bad HTTP code %d — aborting", code);
        http.end();
        return false;
    }

    // ── 3. Save WAV stream directly to LittleFS ───────────
    LittleFS.remove(TTS_WAV_PATH);
    File f = LittleFS.open(TTS_WAV_PATH, FILE_WRITE);
    if (!f) {
        LOG_ERROR("AI-TTS", "Cannot open %s for writing", TTS_WAV_PATH);
        http.end();
        return false;
    }

    size_t bytesWritten = http.writeToStream(&f);
    f.close();
    http.end();

    LOG_INFO("AI-TTS", "WAV saved: %s  bytes=%u", TTS_WAV_PATH, bytesWritten);

    if (bytesWritten == 0) {
        LOG_ERROR("AI-TTS", "Nothing written to LittleFS — aborting");
        return false;
    }

    // ── 4. Play with Audio library ────────────────────────
    // The Audio lib installs its own I2S driver. We must uninstall the
    // one from speakerInit() first, then reinstall it after playback.
    speakerTTSPlaying = true;
    i2s_driver_uninstall(SPK_I2S_PORT);   // hand I2S port to Audio lib

    Audio ttsAudio;
    ttsAudio.setPinout(SPK_I2S_BCLK, SPK_I2S_LRC, SPK_I2S_DIN);
    ttsAudio.setVolume(21);
    ttsAudio.connecttoFS(LittleFS, TTS_WAV_PATH);

    LOG_INFO("AI-TTS", "Playing %s ...", TTS_WAV_PATH);
    while (ttsAudio.isRunning()) {
        ttsAudio.loop();
        esp_task_wdt_reset();
        vTaskDelay(1);
    }

    // Audio destructor releases I2S — now reinstall our own driver
    speakerInit();   // restores I2S_NUM_1 at SPK_SAMPLE_RATE for alarm/buzzer
    speakerTTSPlaying = false;
    LOG_INFO("AI-TTS", "Playback done");
    return true;
}
