# TFT Display + Audio Quote Synchronization

## Overview
When the TFT display shows a motivational quote (during high stress), the system now automatically plays the corresponding audio file.

## How It Works

### 1. Quote Configuration (`config.h`)
- **AUDIO_QUOTE_MAP**: Maps quote text to audio file paths
- **QUOTES[]**: Array of quotes for TFT display (must match map keys)

```cpp
static std::map<std::string, std::string> AUDIO_QUOTE_MAP = {
    {"Take a deep breath", "/q1.mp3"},
    {"This feeling will pass", "/q2.mp3"},
    // ... more quotes
};

static const char* QUOTES[NUM_QUOTES] = {
    "Take a deep breath",
    "This feeling will pass",
    // ... same quotes as map keys
};
```

### 2. TFT Display Logic (`tft_display.cpp`)
When `tftUpdateStress()` detects "High" stress:
1. Displays "HIGH STRESS" in red
2. Picks a random quote from `QUOTES[]`
3. Displays the quote on the TFT
4. Looks up the audio file using `getQuoteByText()`
5. Plays the audio using `playQuote()`

```cpp
int quoteIndex = random(0, NUM_QUOTES);
const char* selectedQuote = QUOTES[quoteIndex];
tft.println(selectedQuote);  // Show on TFT

const AudioQuote* audioQuote = getQuoteByText(selectedQuote);
if (audioQuote) {
    playQuote(audioQuote);  // Play matching audio
}
```

### 3. Audio Playback (`audio_quotes.cpp`)
- Uses ESP32-audioI2S library for non-blocking MP3 playback
- Audio files stored in LittleFS filesystem
- `audioQuotesLoop()` must be called in main loop for audio processing

## Adding New Quotes

### Step 1: Create MP3 File
Create your audio file and name it (e.g., `new_quote.mp3`)

### Step 2: Update `config.h`
Add entry to both the map and array:

```cpp
static std::map<std::string, std::string> AUDIO_QUOTE_MAP = {
    // ... existing quotes
    {"Your new quote text", "/new_quote.mp3"}
};

#define NUM_QUOTES 7  // Increment the count

static const char* QUOTES[NUM_QUOTES] = {
    // ... existing quotes
    "Your new quote text"  // Must match map key EXACTLY
};
```

### Step 3: Upload to LittleFS
Place `new_quote.mp3` in the `data/` folder, then:
```bash
pio run --target uploadfs
```

## Flow Diagram

```
High Stress Detected (GSR)
         ↓
   tftUpdateStress("High")
         ↓
   Pick random quote
         ↓
   Display on TFT ← → Play audio file
         ↓              ↓
   User sees quote   User hears quote
```

## Key Features
✅ **Synchronized**: TFT and audio always show/play the same quote
✅ **Non-blocking**: Audio plays in background via `audioQuotesLoop()`
✅ **Simple config**: Just update `config.h` map and array
✅ **Automatic lookup**: System finds audio file by quote text

## Technical Details
- **Quote matching**: Uses `getQuoteByText()` for exact string matching
- **Audio format**: MP3 files in LittleFS
- **Playback**: ESP32-audioI2S library with I2S output (MAX98357A)
- **Thread safety**: TFT mutex unlocked before audio call
- **Logging**: Quote playback logged for debugging

## Testing
1. Trigger high stress (GSR < 2000 Ω)
2. Verify quote appears on TFT
3. Verify corresponding audio plays through speaker
4. Check serial logs for confirmation:
   ```
   [TFT] Playing audio for quote: 'Take a deep breath'
   ```
