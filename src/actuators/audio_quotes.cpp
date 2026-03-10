// =============================================================
// Audio Quotes Manager — Implementation
// =============================================================
#include "../config.h"
#include "audio_quotes.h"
#include "speaker.h"
#include "../network/logger.h"
#include <LittleFS.h>
#include <Audio.h>
#include <vector>
#include <map>

// Audio object for playing MP3 files.
// MUST use I2S_NUM_1 (the speaker port) — I2S_NUM_0 is reserved for the
// microphone (legacy driver/i2s.h).  Created lazily via pointer so the
// constructor doesn't grab I2S0 at C++ static-init time.
static Audio* audioPlayer = nullptr;
static bool   audioPlayerInitialized = false;

// Audio quotes database
static std::vector<AudioQuote> audioQuotes;
static std::map<QuoteCategory, std::vector<int>> categoryIndex;
static bool initialized = false;

// Helper function to add a quote
static void addQuote(const char* text, const char* audioFile, QuoteCategory category, uint16_t durationMs) {
    int index = audioQuotes.size();
    audioQuotes.push_back({text, audioFile, category, durationMs});
    categoryIndex[category].push_back(index);
}

void audioQuotesInit() {
    if (initialized) {
        LOG_INFO("AQ", "Audio quotes already initialized");
        return;
    }

    LOG_INFO("AQ", "Initializing audio quotes system...");
    
    // Initialize LittleFS if not already done
    if (!LittleFS.begin(true)) {
        LOG_ERROR("AQ", "Failed to mount LittleFS");
        return;
    }

    // Initialize Audio library for MP3 playback on I2S port 1 (speaker)
    if (!audioPlayerInitialized) {
        audioPlayer = new Audio(I2S_NUM_1);   // ← I2S1 = speaker, I2S0 = mic
        audioPlayer->setPinout(SPK_I2S_BCLK, SPK_I2S_LRC, SPK_I2S_DIN);
        audioPlayer->setVolume(21);  // 0-21
        audioPlayer->forceMono(true);
        audioPlayer->setI2SCommFMT_LSB(false);
        audioPlayer->i2s_mclk_pin_select(I2S_PIN_NO_CHANGE);
        audioPlayerInitialized = true;
        LOG_INFO("AQ", "Audio player initialized for MP3 playback (I2S port 1)");
    }

    audioQuotes.clear();
    categoryIndex.clear();

    // Build quotes from the hashmap in config.h
    // All quotes go into QUOTE_CALM category for simplicity
    for (const auto& pair : AUDIO_QUOTE_MAP) {
        const char* quoteText = pair.first.c_str();
        const char* audioPath = pair.second.c_str();
        addQuote(quoteText, audioPath, QUOTE_CALM, 3000);
        LOG_INFO("AQ", "Mapped: '%s' -> %s", quoteText, audioPath);
    }

    initialized = true;
    LOG_INFO("AQ", "Audio quotes initialized: %d total quotes, %d categories", 
             audioQuotes.size(), categoryIndex.size());
    
    // Log category counts
    for (const auto& pair : categoryIndex) {
        LOG_INFO("AQ", "  Category %d: %d quotes", pair.first, pair.second.size());
    }
}

const AudioQuote* getRandomQuote(QuoteCategory category) {
    if (!initialized) {
        LOG_ERROR("AQ", "Audio quotes not initialized");
        return nullptr;
    }

    auto it = categoryIndex.find(category);
    if (it == categoryIndex.end() || it->second.empty()) {
        LOG_WARN("AQ", "No quotes found for category %d", category);
        return nullptr;
    }

    // Get random quote from category
    int randomIdx = it->second[random(0, it->second.size())];
    LOG_INFO("AQ", "Selected quote %d from category %d", randomIdx, category);
    return &audioQuotes[randomIdx];
}

const AudioQuote* getQuoteByText(const char* searchText) {
    if (!initialized || !searchText) {
        LOG_ERROR("AQ", "Invalid parameters for getQuoteByText");
        return nullptr;
    }

    String search = String(searchText);
    search.toLowerCase();

    for (size_t i = 0; i < audioQuotes.size(); i++) {
        String quoteText = String(audioQuotes[i].text);
        quoteText.toLowerCase();
        if (quoteText.indexOf(search) >= 0) {
            LOG_INFO("AQ", "Found quote match: '%s'", audioQuotes[i].text);
            return &audioQuotes[i];
        }
    }

    LOG_WARN("AQ", "No quote found matching '%s'", searchText);
    return nullptr;
}

bool audioFileExists(const char* filepath) {
    if (!filepath) return false;
    bool exists = LittleFS.exists(filepath);
    if (!exists) {
        LOG_WARN("AQ", "Audio file not found: %s", filepath);
    }
    return exists;
}

bool playQuote(const AudioQuote* quote) {
    if (!quote) {
        LOG_ERROR("AQ", "playQuote called with null quote");
        return false;
    }

    LOG_INFO("AQ", "Playing quote: '%s' from %s", quote->text, quote->audioFile);

    // Check if audio file exists
    if (!audioFileExists(quote->audioFile)) {
        LOG_ERROR("AQ", "Audio file missing: %s - falling back to text display", quote->audioFile);
        // Could trigger TFT display of text here
        return false;
    }

    // Use Audio library to play MP3 from LittleFS (like the speaker example)
    LOG_INFO("AQ", "Starting MP3 playback from LittleFS...");
    audioPlayer->connecttoFS(LittleFS, quote->audioFile);

    LOG_INFO("AQ", "Quote playback started (non-blocking)");
    return true;
}

// Add function to handle audio loop processing
void audioQuotesLoop() {
    if (audioPlayerInitialized && audioPlayer) {
        audioPlayer->loop();
    }
}

// Check if audio is currently playing
bool audioQuotesIsPlaying() {
    if (!audioPlayerInitialized || !audioPlayer) return false;
    return audioPlayer->isRunning();
}

bool playQuoteByCategory(QuoteCategory category) {
    const AudioQuote* quote = getRandomQuote(category);
    if (!quote) {
        return false;
    }
    return playQuote(quote);
}

void listAllQuotes() {
    if (!initialized) {
        LOG_ERROR("AQ", "Audio quotes not initialized");
        return;
    }

    LOG_INFO("AQ", "=== AUDIO QUOTES DATABASE ===");
    LOG_INFO("AQ", "Total quotes: %d", audioQuotes.size());
    
    for (size_t i = 0; i < audioQuotes.size(); i++) {
        const AudioQuote& q = audioQuotes[i];
        bool exists = audioFileExists(q.audioFile);
        LOG_INFO("AQ", "[%d] Cat:%d '%s' -> %s (%dms) [%s]", 
                 i, q.category, q.text, q.audioFile, q.durationMs,
                 exists ? "OK" : "MISSING");
    }
    
    LOG_INFO("AQ", "=== END ===");
}
