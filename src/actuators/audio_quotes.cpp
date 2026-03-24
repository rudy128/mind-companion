// =============================================================
// This file manages audio playback. 
// It uses the Audio library to play MP3 files stored in LittleFS.
// =============================================================
#include "audio_quotes.h"
#include "../config.h"
#include <Audio.h>
#include <LittleFS.h>
#include "driver/i2s.h"

// ===== I2S PINS for MAX98357 Amplifier=====
#define I2S_BCLK 21
#define I2S_LRC  0
#define I2S_DIN  14

// Dynamic Audio object pointer
static Audio* pAudio = nullptr;

// FreeRTOS Task handle for audio (Core 0)
static TaskHandle_t audioTaskHandle = nullptr;
static volatile bool audioTaskRunning = false;
static volatile bool audioPaused = false;

// Queue for file requests
static volatile bool playRequested = false;
static char requestedFile[64] = "";

// Loop mode — audio restarts automatically when it finishes
static volatile bool loopMode = false;
static char loopFile[64] = "";

// Signal to stop audio without causing errors
static volatile bool stopRequested = false;

static void initAudioObject();

// Audio task runs on Core 0 to prevent blocking sensors and main loop on Core 1
void audioTask(void* param) {
  Serial.println("[AUDIO] Task started on Core 0");
  audioTaskRunning = true;             //Indicates task is active
  bool prevRunning = false;            // Tracks previous playback state
  
  for (;;) {
    if (audioPaused || pAudio == nullptr) {   // If paused or not initialized, ensure audio is stopped and skip processing
      prevRunning = false;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // Stop is handled inside this task
    if (stopRequested) {
      stopRequested = false;
      if (pAudio != nullptr) {
        pAudio->stopSong();
      }
      prevRunning = false;
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    
    // Check if a new file play was requested
    if (playRequested && pAudio != nullptr) {
      playRequested = false;
      if (strlen(requestedFile) > 0) {
        Serial.print("[AUDIO] Playing: ");
        Serial.println(requestedFile);
        pAudio->connecttoFS(LittleFS, requestedFile);
      }
    }
    
    // Process audio
    if (pAudio != nullptr) {
      pAudio->loop();
      
      bool nowRunning = pAudio->isRunning();
      // Detect transition: Audio was playing or just stopped
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

// Initialize/reinitialize the Audio object
static void initAudioObject() {
  if (pAudio != nullptr) {
    delete pAudio;
    pAudio = nullptr;
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  
  //Create new audio object
  pAudio = new Audio();   

  //Configure I2S pins and playback settings
  pAudio->setPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
  pAudio->setVolume(40);
  pAudio->forceMono(true);
  pAudio->setI2SCommFMT_LSB(false);
  pAudio->i2s_mclk_pin_select(I2S_PIN_NO_CHANGE);
  
  Serial.println("[AUDIO] Audio object initialized");
}

// Stop audio without destroying the I2S driver
void audioQuotesStop() {
  Serial.println("[AUDIO] Stopping playback...");
  loopMode = false;
  loopFile[0] = '\0';
  playRequested = false;
  stopRequested = true;
}

// Pause audio and FULLY release I2S0 for microphone
void audioQuotesPause() {
  if (audioPaused) return;
  
  Serial.println("[AUDIO] Destroying audio for mic recording...");
  audioPaused = true;
  playRequested = false; // Clear any pending requests
  
  // Stop playback
  if (pAudio != nullptr) {
    pAudio->stopSong();
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Delete the Audio object - this releases I2S
    delete pAudio;
    pAudio = nullptr;
  }
  
  // Force uninstall in case Audio didn't clean up (Fails silently if already uninstalled)
  i2s_driver_uninstall(I2S_NUM_0);
  
  vTaskDelay(pdMS_TO_TICKS(100));
  Serial.println("[AUDIO] I2S0 released for mic");
}

// Resume audio after mic recording
void audioQuotesResume() {
  if (!audioPaused) return;
  
  Serial.println("[AUDIO] Recreating audio system...");
  
  // Recreate the fresh Audio object 
  initAudioObject();
  
  audioPaused = false;
  Serial.println("[AUDIO] Audio system restored");
}

// Check if audio is paused
bool audioQuotesIsPaused() {
  return audioPaused;
}

void audioQuotesInit() {
  // Initialize LittleFS first
  if(!LittleFS.begin(true)){
      Serial.println("LittleFS failed");
      return;
  }
  Serial.println("LittleFS initialized successfully");

  // List files
  Serial.println("Files in LittleFS:");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while(file){
    Serial.print("  Found file: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }

  // Create initial Audio object
  initAudioObject();

  // Start audio task on Core 0
  xTaskCreatePinnedToCore(
    audioTask,
    "audio",
    8192,
    nullptr,
    2,
    &audioTaskHandle,
    0
  );
  
  Serial.println("[AUDIO] Init complete, task spawned on Core 0");
}

void audioQuotesLoop() {
  // Empty - task handles it
}

void playFile(const char* filename) {
  if(!LittleFS.exists(filename)){
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
    return;  // already looping this file
  }

  if(!LittleFS.exists(filename)){
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

// Play audio based on quote text from hashmap
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

// Calm quote keys (must match AUDIO_QUOTE_MAP in config.h) for random pick
static const char* const CALM_QUOTES[] = {
  "Take a deep breath",
  "This feeling will pass",
  "Slow down, you got this",
};
static const int NUM_CALM = sizeof(CALM_QUOTES) / sizeof(CALM_QUOTES[0]);

bool playQuoteByCategory(QuoteCategory category) {
  if (category == QUOTE_CALM && NUM_CALM > 0) {
    int idx = random(0, NUM_CALM);
    return playQuoteByHashmap(CALM_QUOTES[idx]);
  }
  // Fallback for other categories (e.g. emergency/breathe)
  playFile(ALARM_AUDIO_FILE);
  return true;
}

bool playAudioFile(const char* filepath) {
  playFile(filepath);
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

bool audioFileExists(const char* filepath) {
  return filepath && LittleFS.exists(filepath);
}

const AudioQuote* getRandomQuote(QuoteCategory category) {
  return nullptr;
}

bool playQuote(const AudioQuote* quote) {
  if (!quote) return false;
  playFile(quote->audioFile);
  return true;
}

const char* audioQuotesGetCurrentText() {
  return "";
}
