// =============================================================
// Audio Quotes — EXACT copy of working script + hashmap lookup
// Runs on Core 0 as dedicated task for smooth playback
// =============================================================
#include "audio_quotes.h"
#include "../config.h"
#include <Audio.h>
#include <LittleFS.h>

// ===== I2S PINS for MAX98357 =====
#define I2S_BCLK 21
#define I2S_LRC  0
#define I2S_DIN  14

Audio audio;

// Task handle for audio on Core 0
static TaskHandle_t audioTaskHandle = nullptr;
static volatile bool audioTaskRunning = false;

// Queue for file requests
static volatile bool playRequested = false;
static char requestedFile[64] = "";

void playFile(const char* filename);

// Audio task runs on Core 0 - just calls audio.loop() continuously
void audioTask(void* param) {
  Serial.println("[AUDIO] Task started on Core 0");
  audioTaskRunning = true;
  
  for (;;) {
    // Check if a new file play was requested
    if (playRequested) {
      playRequested = false;
      if (strlen(requestedFile) > 0) {
        Serial.print("[AUDIO] Playing: ");
        Serial.println(requestedFile);
        audio.connecttoFS(LittleFS, requestedFile);
      }
    }
    
    // This is the critical part - must run frequently
    audio.loop();
    
    // Small delay to prevent watchdog issues
    vTaskDelay(1);
  }
}

void audioQuotesInit() {
  // I2S setup
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
  audio.setVolume(40); // Increase volume (0-21)

  audio.forceMono(true);        // mono speaker
  audio.setI2SCommFMT_LSB(false);
  // Optional: Set I2S buffer size for smoother playback
  audio.i2s_mclk_pin_select(I2S_PIN_NO_CHANGE);
  audio.forceMono(true); // Force mono if your speaker is mono
   
  // Initialize LittleFS
  if(!LittleFS.begin(true)){
      Serial.println("LittleFS failed");
      return;
  }
  Serial.println("LittleFS initialized successfully");

  // List all files in LittleFS for debugging
  Serial.println("Files in LittleFS:");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while(file){
    Serial.print("  Found file: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }

  // Start audio task on Core 0
  xTaskCreatePinnedToCore(
    audioTask,          // function
    "audio",            // name
    8192,               // stack size
    nullptr,            // param
    2,                  // priority (higher than idle)
    &audioTaskHandle,   // handle
    0                   // Core 0
  );
  
  Serial.println("[AUDIO] Init complete, task spawned on Core 0");
}

void audioQuotesLoop() {
  // No longer needed - task handles it
  // Keep empty for compatibility
}

void playFile(const char* filename) {
  // Check if file exists in LittleFS
  if(!LittleFS.exists(filename)){
    Serial.print("File not found in LittleFS: ");
    Serial.println(filename);
    return;
  }

  // Request the task to play this file
  strncpy(requestedFile, filename, sizeof(requestedFile) - 1);
  requestedFile[sizeof(requestedFile) - 1] = '\0';
  playRequested = true;
  
  Serial.print("[AUDIO] Queued: ");
  Serial.println(filename);
}

// Play audio based on quote text from hashmap
bool playQuoteByHashmap(const char* quoteText) {
  if (!quoteText) {
    Serial.println("playQuoteByHashmap: null text");
    return false;
  }
  
  // Look up in hashmap from config.h
  std::string key(quoteText);
  auto it = AUDIO_QUOTE_MAP.find(key);
  if (it == AUDIO_QUOTE_MAP.end()) {
    Serial.print("Quote not in hashmap: ");
    Serial.println(quoteText);
    return false;
  }
  
  const char* audioPath = it->second.c_str();
  Serial.print("Hashmap lookup: '");
  Serial.print(quoteText);
  Serial.print("' -> ");
  Serial.println(audioPath);
  
  playFile(audioPath);
  return true;
}

// Play by category - just plays q1.mp3 for now
bool playQuoteByCategory(QuoteCategory category) {
  playFile("/q1.mp3");
  return true;
}

// Direct file play
bool playAudioFile(const char* filepath) {
  playFile(filepath);
  return true;
}

// Test play on boot
void audioQuotesTestPlay() {
  Serial.println("TEST: Playing /q1.mp3...");
  playFile("/q1.mp3");
}

// Check if playing
bool audioQuotesIsPlaying() {
  return audio.isRunning();
}

// Stub functions for compatibility
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
