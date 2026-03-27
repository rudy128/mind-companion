// =============================================================
// This file manages audio playback.
// It uses the Audio library to play MP3 files stored in LittleFS.
// =============================================================
#include "audio_quotes.h"
#include "../config.h"
#include <string>
#include <Audio.h>
#include <LittleFS.h>
#include "driver/i2s.h"

#define I2S_BCLK 21
#define I2S_LRC  0
#define I2S_DIN  14

static Audio* pAudio = nullptr;

static TaskHandle_t audioTaskHandle = nullptr;
static volatile bool audioPaused = false;

static volatile bool playRequested = false;
static char requestedFile[64] = "";

static volatile bool loopMode = false;
static char loopFile[64] = "";

static volatile bool stopRequested = false;

static void initAudioObject();

void audioTask(void* param) {
  Serial.println("[AUDIO] Task started on Core 0");
  bool prevRunning = false;

  for (;;) {
    if (audioPaused || pAudio == nullptr) {
      prevRunning = false;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    if (stopRequested) {
      stopRequested = false;
      if (pAudio != nullptr) {
        pAudio->stopSong();
      }
      prevRunning = false;
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    if (playRequested && pAudio != nullptr) {
      playRequested = false;
      if (strlen(requestedFile) > 0) {
        Serial.print("[AUDIO] Playing: ");
        Serial.println(requestedFile);
        pAudio->connecttoFS(LittleFS, requestedFile);
      }
    }

    if (pAudio != nullptr) {
      pAudio->loop();

      bool nowRunning = pAudio->isRunning();
      if (prevRunning && !nowRunning && loopMode && strlen(loopFile) > 0) {
        Serial.print("[AUDIO] Looping: ");
        Serial.println(loopFile);
        pAudio->connecttoFS(LittleFS, loopFile);
        nowRunning = true;
      }
      prevRunning = nowRunning;
    }

    vTaskDelay(1);
  }
}

static void initAudioObject() {
  if (pAudio != nullptr) {
    delete pAudio;
    pAudio = nullptr;
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  pAudio = new Audio();
  pAudio->setPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
  pAudio->setVolume(40);
  pAudio->forceMono(true);
  pAudio->setI2SCommFMT_LSB(false);
  pAudio->i2s_mclk_pin_select(I2S_PIN_NO_CHANGE);

  Serial.println("[AUDIO] Audio object initialized");
}

void audioQuotesStop() {
  Serial.println("[AUDIO] Stopping playback...");
  loopMode = false;
  loopFile[0] = '\0';
  playRequested = false;
  stopRequested = true;
}

void audioQuotesPause() {
  if (audioPaused) return;

  Serial.println("[AUDIO] Destroying audio for mic recording...");
  audioPaused = true;
  playRequested = false;

  if (pAudio != nullptr) {
    pAudio->stopSong();
    vTaskDelay(pdMS_TO_TICKS(50));
    delete pAudio;
    pAudio = nullptr;
  }

  i2s_driver_uninstall(I2S_NUM_0);

  vTaskDelay(pdMS_TO_TICKS(100));
  Serial.println("[AUDIO] I2S0 released for mic");
}

void audioQuotesResume() {
  if (!audioPaused) return;

  Serial.println("[AUDIO] Recreating audio system...");
  initAudioObject();
  audioPaused = false;
  Serial.println("[AUDIO] Audio system restored");
}

void audioQuotesInit() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS failed");
    return;
  }
  Serial.println("LittleFS initialized successfully");

  Serial.println("Files in LittleFS:");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.print("  Found file: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }

  initAudioObject();

  xTaskCreatePinnedToCore(
    audioTask,
    "audio",
    8192,
    nullptr,
    2,
    &audioTaskHandle,
    0);

  Serial.println("[AUDIO] Init complete, task spawned on Core 0");
}

void playFile(const char* filename) {
  if (!LittleFS.exists(filename)) {
    Serial.print("File not found: ");
    Serial.println(filename);
    return;
  }

  loopMode = false;
  loopFile[0] = '\0';

  strncpy(requestedFile, filename, sizeof(requestedFile) - 1);
  requestedFile[sizeof(requestedFile) - 1] = '\0';
  playRequested = true;

  Serial.print("[AUDIO] Queued: ");
  Serial.println(filename);
}

void playFileLooped(const char* filename) {
  if (loopMode && strcmp(loopFile, filename) == 0) {
    return;
  }

  if (!LittleFS.exists(filename)) {
    Serial.print("File not found: ");
    Serial.println(filename);
    return;
  }

  strncpy(loopFile, filename, sizeof(loopFile) - 1);
  loopFile[sizeof(loopFile) - 1] = '\0';
  loopMode = true;

  strncpy(requestedFile, filename, sizeof(requestedFile) - 1);
  requestedFile[sizeof(requestedFile) - 1] = '\0';
  playRequested = true;

  Serial.print("[AUDIO] Loop started: ");
  Serial.println(filename);
}

bool playQuoteByHashmap(const char* quoteText) {
  if (!quoteText) return false;

  std::string key(quoteText);
  auto it = AUDIO_QUOTE_MAP.find(key);
  if (it == AUDIO_QUOTE_MAP.end()) {
    Serial.print("Quote not in hashmap: ");
    Serial.println(quoteText);
    return false;
  }

  playFile(it->second.c_str());
  return true;
}

void audioQuotesTestPlay() {
  Serial.print("TEST: Playing ");
  Serial.println(ALARM_AUDIO_FILE);
  playFile(ALARM_AUDIO_FILE);
}

bool audioQuotesIsPlaying() {
  return playRequested || (pAudio != nullptr && pAudio->isRunning());
}
