# Audio System Fixed: Now Using MP3 with Audio Library ✅

## Problem Identified
The audio quotes system was trying to play WAV files using raw PCM playback (`speakerPlayRaw()`), which required:
- Reading entire file into memory
- Manual buffer management  
- Complex error handling
- Only supported WAV/PCM format

## Solution Implemented
Changed to use the **ESP32-audioI2S Audio library** (same as speaker example) which:
- ✅ Plays MP3 files directly from LittleFS
- ✅ No buffer management needed
- ✅ Non-blocking playback
- ✅ Supports MP3, WAV, AAC, and more
- ✅ Simple API: `audio.connecttoFS(LittleFS, "/file.mp3")`

## Changes Made

### 1. **`audio_quotes.cpp`** - Complete Rewrite

**Added:**
```cpp
#include <Audio.h>

static Audio audioPlayer;
static bool audioPlayerInitialized = false;
```

**Audio Player Initialization:**
```cpp
audioPlayer.setPinout(SPK_I2S_BCLK, SPK_I2S_LRC, SPK_I2S_DIN);
audioPlayer.setVolume(21);
audioPlayer.forceMono(true);
audioPlayer.setI2SCommFMT_LSB(false);
audioPlayer.i2s_mclk_pin_select(I2S_PIN_NO_CHANGE);
```

**New Playback Method:**
```cpp
bool playQuote(const AudioQuote* quote) {
    // Check if file exists
    if (!audioFileExists(quote->audioFile)) return false;
    
    // Play MP3 directly from LittleFS
    audioPlayer.connecttoFS(LittleFS, quote->audioFile);
    
    return true;
}
```

**New Functions:**
```cpp
void audioQuotesLoop() {
    audioPlayer.loop();  // Must be called in main loop
}

bool audioQuotesIsPlaying() {
    return audioPlayer.isRunning();
}
```

**Removed:**
- Manual file reading
- Buffer malloc/free
- Raw PCM streaming to I2S
- ~60 lines of complex buffer management code

### 2. **`audio_quotes.h`** - Added Loop Function

```cpp
void audioQuotesLoop();        // Process audio playback
bool audioQuotesIsPlaying();   // Check if playing
```

### 3. **`main.cpp`** - Added Loop Call

```cpp
void loop() {
    unsigned long now = millis();
    
    // ── Audio Quotes Loop — process MP3 playback ─────────
    audioQuotesLoop();
    
    // ... rest of loop
}
```

### 4. **All Quotes Changed from WAV to MP3**

```cpp
// Before
addQuote("Relax...", "/audio/relax.wav", QUOTE_CALM, 3000);

// After  
addQuote("Relax...", "/audio/relax.mp3", QUOTE_CALM, 3000);
```

All 18 quotes now use `.mp3` extension.

### 5. **Documentation Updated**

- `QUICKSTART_AUDIO_QUOTES.md` - Updated for MP3 format
- Removed WAV conversion instructions
- Added MP3 TTS service recommendations

## How It Works Now

### GSR High Stress Flow
```
1. GSR detects high stress (conductance < 2000)
   ↓
2. playQuoteByCategory(QUOTE_CALM)
   ↓
3. Random selection from 3 CALM MP3 files:
   - /audio/relax.mp3
   - /audio/take_it_easy.mp3
   - /audio/breathe_peace.mp3
   ↓
4. audioPlayer.connecttoFS(LittleFS, "/audio/relax.mp3")
   ↓
5. Audio library starts MP3 playback (non-blocking)
   ↓
6. Main loop calls audioQuotesLoop() every iteration
   ↓
7. audioPlayer.loop() decodes & streams MP3 to I2S
   ↓
8. Speaker plays audio
```

### Same as Speaker Example
```cpp
// Speaker example (vsls:/~3/src/main.cpp)
audio.connecttoFS(LittleFS, "/daksh.mp3");

// Audio quotes (now identical approach)
audioPlayer.connecttoFS(LittleFS, "/audio/relax.mp3");
```

## Benefits

### MP3 vs WAV
| Feature | MP3 | WAV |
|---------|-----|-----|
| File Size | ~50KB | ~500KB |
| Quality | Excellent | Perfect |
| Decode | Hardware | None needed |
| Buffer | Small chunks | Entire file |
| Memory | Low | High |

### Code Simplification
- **Before**: 60+ lines (malloc, read, stream, free)
- **After**: 3 lines (check exists, connecttoFS, return)
- **Reduction**: ~95% simpler

### Reliability
- ✅ No malloc failures
- ✅ No buffer overflow risks
- ✅ No incomplete reads
- ✅ Library handles all edge cases

## File Format Requirements

### MP3 Files
- **Format**: MP3 (MPEG Audio Layer 3)
- **Bitrate**: 128 kbps or higher recommended
- **Sample Rate**: Any (library handles conversion)
- **Channels**: Mono or Stereo (auto-converted to mono)
- **Size**: Smaller is better (~50KB typical)

### Creating MP3 Files

**Option 1: TTS Services (Easiest)**
```
TTSMaker.com → Download as MP3 → Place in data/audio/
```

**Option 2: Convert existing audio**
```bash
ffmpeg -i input.wav -codec:a libmp3lame -qscale:a 2 output.mp3
```

**Option 3: OpenAI TTS (one-time)**
```bash
curl https://api.openai.com/v1/audio/speech \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"model":"tts-1","voice":"alloy","input":"Relax","response_format":"mp3"}' \
  --output relax.mp3
```

## Required Files

Place these 18 MP3 files in `data/audio/`:

**CALM** (3):
- relax.mp3
- take_it_easy.mp3
- breathe_peace.mp3

**MOTIVATE** (3):
- stronger.mp3
- keep_going.mp3
- believe.mp3

**BREATHE** (3):
- breathe_together.mp3
- deep_breath.mp3
- focus_breathing.mp3

**EMERGENCY** (3):
- safe.mp3
- emergency.mp3
- monitoring.mp3

**AWAKE** (3):
- stay_awake.mp3
- wake_up.mp3
- drowsy.mp3

**GREETING** (3):
- hello.mp3
- good_morning.mp3
- welcome.mp3

## Upload Process

```bash
# 1. Place MP3 files in data/audio/
cd "Merged Code"
ls data/audio/  # Verify 18 MP3 files

# 2. Upload filesystem
pio run --target uploadfs

# 3. Upload firmware
pio run --target upload

# 4. Monitor
pio device monitor
```

## Expected Serial Output

```
[INFO] AQ: Initializing audio quotes system...
[INFO] AQ: Audio player initialized for MP3 playback
[INFO] AQ: Audio quotes initialized: 18 total quotes, 6 categories
[INFO] AQ:   Category 0: 3 quotes
[WARN] GSR: HIGH STRESS DETECTED (1800.0 < 2000.0)
[INFO] AQ: Selected quote 0 from category 0
[INFO] AQ: Playing quote: 'Relax. Slow down. You got this.' from /audio/relax.mp3
[INFO] AQ: Starting MP3 playback from LittleFS...
[INFO] AQ: Quote playback started (non-blocking)
[INFO] GSR: Playing calming audio quote due to high stress
```

## Testing

### Test 1: File Exists Check
```
[INFO] AQ: [0] Cat:0 'Relax...' -> /audio/relax.mp3 (3000ms) [OK]
```
If `[MISSING]`, file not uploaded.

### Test 2: Playback
Trigger high stress (low conductance) and listen for audio.

### Test 3: Loop Processing
Audio plays smoothly without blocking main loop.

## Compilation Status

✅ No errors  
✅ All files updated  
✅ Documentation updated  
✅ Ready to upload

## Summary

The audio quotes system now uses the **exact same method** as your working speaker example:
- Audio library for MP3 playback
- LittleFS filesystem
- Non-blocking loop processing
- Simple, reliable, proven approach

Just add the 18 MP3 files and upload!
