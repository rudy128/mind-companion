// =============================================================
// Audio Quotes — EXACT copy of working script + hashmap lookup
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

void playFile(const char* filename);

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
}

void audioQuotesLoop() {
  audio.loop();
}

void playFile(const char* filename) {
  // Check if file exists in LittleFS
  if(!LittleFS.exists(filename)){
    Serial.print("File not found in LittleFS: ");
    Serial.println(filename);
    return;
  }

  Serial.print("Playing: ");
  Serial.println(filename);
  audio.connecttoFS(LittleFS, filename);
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
