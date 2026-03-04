Full Problem List — All Fixable Issues
🔴 Critical (crashes/data corruption)
1. openaiSpeak() — mutex release BEFORE speakerTTSPlaying = false In openai_api.cpp, after TTS playback, the code does:

```
speakerInit();                   // reinstalls I2S driver
speakerTTSPlaying = false;       // ← still inside the mutex!
if (speakerI2SMutex) xSemaphoreGive(speakerI2SMutex);
```

But speakerInit() calls xSemaphoreCreateMutex() again if speakerI2SMutex is null (it isn't), and more importantly it calls _precomputeAlarmBuffers() which is fine. However, speakerInit() itself creates a NEW I2S driver while we're still holding the mutex — and the old mutex was created by the previous speakerInit() call. This is OK logically, but there's a subtle deadlock risk: speakerInit() internally does nothing with the mutex, so it's safe. But the flag order is wrong — speakerTTSPlaying should be cleared after giving back the mutex so Core 1 doesn't see speakerTTSPlaying == false and try to write before the mutex is released. Actually, it's fine because Core 1 uses xSemaphoreTake(..., 0) which will fail until give. Not a real bug upon closer look.

2. vibrationPulse() is BLOCKING — freezes the entire Core 1 loop vibrationPulse(1000) calls delay(1000) which blocks Core 1 for a full second. During this time, heart rate FIFO overflows, the TFT freezes, alarm/buzzer don't update, and the web server stalls. This is called both from the awake-nudge (1000ms) and the web dashboard vibrate button (800ms).

3. tftUpdateSpeechStatus() and tftShowListening() called from Core 0 speech task These TFT functions are called from the speech task on Core 0. They DO use the TFT mutex, so they won't corrupt SPI, BUT the software SPI for ILI9341 is not DMA-backed and takes significant time. Calling tftShowListening(true) then tftShowListening(false) from Core 0 while Core 1 is doing tftUpdateHeartRate() etc. creates mutex contention that can stall Core 0's speech timing.

4. dashState race conditions — read/written from both cores with NO lock dashState.lastVoiceCommand, dashState.emergencyActive, etc. are written from Core 0 (speech task) and read from Core 1 (web server handler). String assignment is NOT atomic on ESP32. If the web server reads lastVoiceCommand mid-assignment, it can get a corrupted pointer → crash.

5. commandParse() creates a lowercase COPY of the String — wasteful heap allocation on Core 0 String lower = text; lower.toLowerCase(); duplicates the entire transcript string on the heap. On Core 0 where heap is precious, this is risky.

🟠 High (performance/reliability)
6. StaticJsonDocument<256> is deprecated in ArduinoJson v7 In openai_api.cpp line ~178, StaticJsonDocument<256> doc; — you're using ArduinoJson v7.4.2 (per platformio.ini) but StaticJsonDocument was removed in v7. Should be JsonDocument doc;.

7. Response reading in openaiTranscribe() uses response += c one char at a time This is O(n²) String building. Each += may reallocate. For a typical OpenAI response of ~200 bytes this is tolerable, but it fragments the heap. Should read in chunks.

8. speakerBeepOK() is BLOCKING — called from Core 0 speech task speakerBeepOK() calls speakerPlayTone() which blocks for 100ms + 50ms delay + 100ms = 250ms total. When called from the speech task (CMD_HOW_AM_I), it blocks Core 0. It also writes to I2S without holding the mutex.

9. speakerPlayTone() / speakerBeepOK() / speakerBeepError() don't use the I2S mutex These functions write directly to I2S without acquiring speakerI2SMutex, potentially colliding with alarm/buzzer on Core 1 or TTS.

10. GSR stress text mismatch between gsr.cpp and tft_display.cpp gsrGetStressLevel() returns "Please wear finger straps" but tftUpdateStress() checks for "Please wear finger band". They'll never match → the TFT will fall through to the else branch and show "LOW / Calm" when the band isn't worn.

11. ledBreathingStart() ignores the cycles parameter The function takes int cycles but never stores or uses it. The breathing pattern runs forever until ledBreathingStop() is called manually.

12. QUOTES[] array is defined static in a header file (config.h) Every .cpp that #includes config.h gets its own copy of the QUOTES array in RAM. Since config.h is included by ~12 files, that's 12 × (6 pointers + string literals) = wasted RAM. The literals themselves are in flash but the pointer array is duplicated.

13. openaiTranscribe() response timeout is only 10 seconds The while(client.connected() && millis() - timeout < 10000) loop reads the response. If Whisper inference takes >10s (which it can for noisy audio), the response is truncated.

🟡 Medium (correctness/quality)
14. sleepDetectorGetQuality() returns a String by value every call Called every loop iteration (1-second tick), allocates a new String on the heap each time. Should return const char*.

15. rtcGetSecond() / rtcGetHour() / rtcGetMinute() each do a separate I2C read rtcFormatTime() calls rtc.now() once. But the individual getters each call rtc.now() separately — 3 I2C transactions when called together. In the current code only rtcGetSecond() is called separately, so it's just 2 reads per poll. Minor but still wasteful.

16. Camera stream handler has no frame rate limiter streamHandler() captures and sends frames as fast as possible in a tight loop. This maxes out one CPU core and uses maximum WiFi bandwidth. A 10-20ms delay between frames would save both.

17. LittleFS.begin(false) called EVERY TTS call openaiSpeak() mounts LittleFS every time it's called. Should mount once in setup.

18. openaiSpeak() uses HTTPClient which allocates its own SSL buffer Despite the reusable sslClient for openaiTranscribe(), the TTS function uses HTTPClient which creates its own internal WiFiClientSecure → another 32KB of TLS buffers. Should use the shared sslClient.

19. Dead commented-out code blocks in main.cpp Multiple large commented-out blocks cluttering the code.