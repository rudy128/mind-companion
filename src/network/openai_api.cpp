// =============================================================
// OpenAI API — Whisper STT + TTS Implementation
// =============================================================
#include "openai_api.h"
#include "../config.h"
#include "../actuators/speaker.h"
#include "../network/wifi_manager.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <LittleFS.h>
#include <Audio.h>
#include <driver/i2s.h>   // for i2s_driver_uninstall / reinstall around Audio playback

#define TTS_WAV_PATH  "/tts_out.wav"

// =============================================================
//  openaiTranscribe — streaming upload (no 64 KB malloc copy)
//
//  Creates a FRESH WiFiClientSecure each call to avoid the
//  "Bad file number" (errno 9) bug from reusing a stopped client.
//  The ~40KB TLS allocation is freed when the function returns.
// =============================================================
String openaiTranscribe(int16_t* pcmData, size_t pcmBytes) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10);    // 10-second TLS handshake timeout

    Serial.println("[AI] Connecting to api.openai.com...");
    if (!client.connect("api.openai.com", 443)) {
        Serial.println("[AI] Connection failed");
        return "";
    }

    // Build the 44-byte WAV header on the stack
    uint8_t wavHeader[44];
    {
        uint32_t fileSize = pcmBytes + 36;
        memcpy(wavHeader,      "RIFF", 4);
        *(uint32_t*)(wavHeader + 4)  = fileSize;
        memcpy(wavHeader + 8,  "WAVEfmt ", 8);
        *(uint32_t*)(wavHeader + 16) = 16;
        *(uint16_t*)(wavHeader + 20) = 1;               // PCM
        *(uint16_t*)(wavHeader + 22) = 1;               // mono
        *(uint32_t*)(wavHeader + 24) = MIC_SAMPLE_RATE;
        *(uint32_t*)(wavHeader + 28) = MIC_SAMPLE_RATE * 2;
        *(uint16_t*)(wavHeader + 32) = 2;
        *(uint16_t*)(wavHeader + 34) = 16;
        memcpy(wavHeader + 36, "data", 4);
        *(uint32_t*)(wavHeader + 40) = pcmBytes;
    }

    size_t wavTotalSize = 44 + pcmBytes;   // total file size for Content-Length

    String boundary  = "----ESP32Boundary";
    String bodyStart = "--" + boundary + "\r\n"
                       "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
                       "whisper-1\r\n"
                       "--" + boundary + "\r\n"
                       "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
                       "Content-Type: audio/wav\r\n\r\n";
    String bodyEnd   = "\r\n--" + boundary + "--\r\n";

    size_t contentLength = bodyStart.length() + wavTotalSize + bodyEnd.length();

    // HTTP headers
    client.print("POST /v1/audio/transcriptions HTTP/1.1\r\n");
    client.print("Host: api.openai.com\r\n");
    client.print("Authorization: Bearer " + String(OPENAI_API_KEY) + "\r\n");
    client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
    client.print("Content-Length: " + String(contentLength) + "\r\n\r\n");

    // Body — multipart preamble
    client.print(bodyStart);

    // WAV header (44 bytes)
    client.write(wavHeader, 44);
    esp_task_wdt_reset();

    // Stream PCM data directly from audioBuffer — no copy needed
    const size_t chunkSize = 1024;
    const uint8_t* rawBytes = (const uint8_t*)pcmData;
    for (size_t i = 0; i < pcmBytes; i += chunkSize) {
        size_t len = min(chunkSize, pcmBytes - i);
        client.write(rawBytes + i, len);
        esp_task_wdt_reset();
        vTaskDelay(1);
    }

    // Multipart epilogue
    client.print(bodyEnd);

    // Read response in chunks (not char-by-char which is O(n²) on String)
    String response;
    response.reserve(512);
    uint8_t readBuf[256];
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 15000) {
        int avail = client.available();
        if (avail > 0) {
            int toRead = min(avail, (int)sizeof(readBuf));
            int got = client.read(readBuf, toRead);
            if (got > 0) {
                response.concat((const char*)readBuf, got);
                timeout = millis();   // reset timeout on data received
            }
        }
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
    Serial.printf("[AI-TTS] speak() → \"%s\"\n", text);

    // ── 0. Verify WiFi is up before touching HTTPClient ─
    if (!wifiEnsureConnected()) {
        Serial.println("[AI-TTS] WiFi not ready — aborting TTS");
        return false;
    }

    // ── 1. Mount LittleFS ─────────────────────────────────
    if (!LittleFS.begin(false)) {
        Serial.println("[AI-TTS] LittleFS mount failed — formatting...");
        LittleFS.format();
        if (!LittleFS.begin(false)) {
            Serial.println("[AI-TTS] LittleFS init failed");
            return false;
        }
    }

    // ── 2. POST to OpenAI TTS, response_format = wav ─────
    HTTPClient http;
    http.begin("https://api.openai.com/v1/audio/speech");
    http.addHeader("Content-Type",  "application/json");
    http.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);

    JsonDocument doc;
    doc["model"]  = "tts-1";
    doc["voice"]  = "alloy";
    doc["input"]  = text;
    doc["response_format"] = "wav";
    String body;
    serializeJson(doc, body);

    Serial.println("[AI-TTS] POST /v1/audio/speech ...");
    int code = http.POST(body);
    Serial.printf("[AI-TTS] HTTP %d\n", code);
    if (code != 200) {
        Serial.printf("[AI-TTS] Bad HTTP code %d\n", code);
        http.end();
        return false;
    }

    // ── 3. Save WAV stream directly to LittleFS ───────────
    LittleFS.remove(TTS_WAV_PATH);
    File f = LittleFS.open(TTS_WAV_PATH, FILE_WRITE);
    if (!f) {
        Serial.println("[AI-TTS] Cannot open file for writing");
        http.end();
        return false;
    }

    size_t bytesWritten = http.writeToStream(&f);
    f.close();
    http.end();

    Serial.printf("[AI-TTS] WAV saved: %u bytes\n", bytesWritten);

    if (bytesWritten == 0) {
        Serial.println("[AI-TTS] Nothing written — aborting");
        return false;
    }

    // ── 4. Play with Audio library ────────────────────────
    // The Audio lib installs its own I2S driver. We must uninstall the
    // one from speakerInit() first, then reinstall it after playback.
    // Hold the I2S mutex so Core 1 alarm/buzzer don't collide.
    if (speakerI2SMutex) xSemaphoreTake(speakerI2SMutex, portMAX_DELAY);
    speakerTTSPlaying = true;
    i2s_driver_uninstall(SPK_I2S_PORT);   // hand I2S port to Audio lib

    Audio ttsAudio;
    ttsAudio.setPinout(SPK_I2S_BCLK, SPK_I2S_LRC, SPK_I2S_DIN);
    ttsAudio.setVolume(21);
    ttsAudio.connecttoFS(LittleFS, TTS_WAV_PATH);

    Serial.println("[AI-TTS] Playing WAV...");
    while (ttsAudio.isRunning()) {
        ttsAudio.loop();
        esp_task_wdt_reset();
        vTaskDelay(1);
    }

    // Audio destructor releases I2S — now reinstall our own driver
    speakerInit();   // restores I2S_NUM_1 at SPK_SAMPLE_RATE for alarm/buzzer
    speakerTTSPlaying = false;
    if (speakerI2SMutex) xSemaphoreGive(speakerI2SMutex);
    Serial.println("[AI-TTS] Playback done");
    return true;
}
