// =============================================================
// Audio Quotes — Time-sliced I2S sharing with microphone
// Uses dynamic Audio object - destroyed before mic, recreated after
// =============================================================
#include "audio_quotes.h"
#include "../config.h"
#include <Audio.h>
#include <LittleFS.h>
#include "driver/i2s.h"

// ===== I2S PINS for MAX98357 =====
#define I2S_BCLK 21
#define I2S_LRC  0
#define I2S_DIN  14

// Dynamic Audio object - can be destroyed and recreated
static Audio* pAudio = nullptr;

// Task handle for audio on Core 0
static TaskHandle_t audioTaskHandle = nullptr;
static volatile bool audioTaskRunning = false;
static volatile bool audioPaused = false;

// Queue for file requests
static volatile bool playRequested = false;
static char requestedFile[64] = "";

// Forward declaration
static void initAudioObject();

// Audio task runs on Core 0
void audioTask(void* param) {
  Serial.println("[AUDIO] Task started on Core 0");
  audioTaskRunning = true;
  
  for (;;) {
    // If paused (mic is recording), just wait
    if (audioPaused || pAudio == nullptr) {
      vTaskDelay(pdMS_TO_TICKS(50));
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
  
  pAudio = new Audio();
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
  playRequested = false;
  if (pAudio != nullptr) {
    pAudio->stopSong();
  }
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
  
  // Belt-and-suspenders: force uninstall in case Audio didn't clean up
  // (ignore error if already uninstalled)
  i2s_driver_uninstall(I2S_NUM_0);
  
  vTaskDelay(pdMS_TO_TICKS(100));
  Serial.println("[AUDIO] I2S0 released for mic");
}

// Resume audio after mic recording
void audioQuotesResume() {
  if (!audioPaused) return;
  
  Serial.println("[AUDIO] Recreating audio system...");
  
  // Recreate the Audio object fresh
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

  strncpy(requestedFile, filename, sizeof(requestedFile) - 1);
  requestedFile[sizeof(requestedFile) - 1] = '\0';
  playRequested = true;
  
  Serial.print("[AUDIO] Queued: ");
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

bool playQuoteByCategory(QuoteCategory category) {
  playFile("/q1.mp3");
  return true;
}

bool playAudioFile(const char* filepath) {
  playFile(filepath);
  return true;
}

void audioQuotesTestPlay() {
  Serial.println("TEST: Playing /q1.mp3...");
  playFile("/q1.mp3");
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
