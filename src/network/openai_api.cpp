// =============================================================
// OpenAI API — Whisper STT Implementation
// =============================================================
#include "openai_api.h"
#include "../config.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>   // for esp_task_wdt_reset()

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
