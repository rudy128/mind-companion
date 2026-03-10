# Audio Quotes System - Quick Start Guide

## What We Built

A complete audio quote system for the M.I.N.D. Companion that maps motivational quotes to audio files using a hashmap. When voice commands are recognized, the system automatically plays appropriate audio responses.

## How It Works

```
User Voice → OpenAI Transcription → Command Parser → Quote Player → Speaker
     ↓              ↓                      ↓              ↓           ↓
"help me breathe" → "help" → CMD_BREATHING → Random → "/audio/breathe.wav"
```

## What You Need To Do Next

### Step 1: Create Audio Files (IMPORTANT!)

You need to create 18 audio files. Here's the complete list:

#### Calm Quotes (3 files)
- `relax.mp3` - "Relax. Slow down. You got this."
- `take_it_easy.mp3` - "Take it easy. Everything will be fine."
- `breathe_peace.mp3` - "Breathe in peace. Breathe out stress."

#### Motivational (3 files)
- `stronger.mp3` - "You are stronger than you think."
- `keep_going.mp3` - "Keep going. You're doing great."
- `believe.mp3` - "Believe in yourself. You can do this."

#### Breathing (3 files)
- `breathe_together.mp3` - "Let's breathe together. In... and out."
- `deep_breath.mp3` - "Deep breath in. Hold. Now exhale slowly."
- `focus_breathing.mp3` - "Focus on your breathing. In through your nose, out through your mouth."

#### Emergency (3 files)
- `safe.mp3` - "I'm here with you. You're safe."
- `emergency.mp3` - "Emergency support activated. Help is on the way."
- `monitoring.mp3` - "Stay calm. I'm monitoring your vitals."

#### Awake (3 files)
- `stay_awake.mp3` - "Hey, stay with me. Keep your eyes open."
- `wake_up.mp3` - "Time to wake up. Let's keep you alert."
- `drowsy.mp3` - "I noticed you're getting drowsy. Let's move around."

#### Greetings (3 files)
- `hello.mp3` - "Hello! I'm M.I.N.D., your companion. How are you feeling?"
- `good_morning.mp3` - "Good morning! Ready to start your day?"
- `welcome.mp3` - "Welcome back! I'm here to support you."

### Step 2: Convert Audio to Correct Format

**Option A: Using Text-to-Speech Services**
1. Use services like:
   - TTSMaker (https://ttsmaker.com/)
   - Google Cloud TTS
   - Amazon Polly
   - ElevenLabs

2. Download as MP3 directly

3. Place in `data/audio/` folder (no conversion needed!)

**Option B: Convert existing audio to MP3**
```bash
ffmpeg -i input.wav -codec:a libmp3lame -qscale:a 2 output.mp3
```

### Step 3: Place Files in data/audio/
```
Merged Code/
  data/
    audio/
      relax.mp3
      take_it_easy.mp3
      breathe_peace.mp3
      ... (all 18 MP3 files)
```

### Step 4: Upload Filesystem
```bash
cd "Merged Code"
pio run --target uploadfs
```

### Step 5: Upload Firmware
```bash
pio run --target upload
```

### Step 6: Monitor Serial Output
```bash
pio device monitor
```

Look for:
```
[INFO] AQ: Audio quotes initialized: 18 total quotes, 6 categories
[INFO] AQ: [0] Cat:0 'Relax. Slow down. You got this.' -> /audio/relax.mp3 (3000ms) [OK]
...
```

## Testing Voice Commands

Say these phrases to test:

| Phrase | Expected Result |
|--------|----------------|
| "I need help breathing" | Plays random breathing exercise |
| "How am I doing?" | Plays calming quote |
| "Help me" | Plays calming quote |
| "Emergency" | Plays emergency quote + activates alarm |
| "Stop" | Stops all audio/alarms |

## If Audio Files Are Missing

The system will gracefully handle missing files:
```
[WARN] AQ: Audio file not found: /audio/relax.mp3
[ERROR] AQ: Audio file missing: /audio/relax.mp3 - falling back to text display
```

The system will continue to work, just without audio playback.

## File Locations

### Source Code
- `src/actuators/audio_quotes.h` - API definitions
- `src/actuators/audio_quotes.cpp` - Implementation (EDIT HERE to add/change quotes)
- `src/logic/command_handler.cpp` - Command integration

### Documentation
- `docs/AUDIO_QUOTES.md` - Complete technical documentation
- `docs/IMPLEMENTATION_AUDIO_QUOTES.md` - Implementation summary
- `data/audio/README.md` - Audio file requirements

### Tools
- `scripts/convert_audio.sh` - Audio conversion helper

## Adding New Quotes

1. **Edit** `src/actuators/audio_quotes.cpp`:
   ```cpp
   void audioQuotesInit() {
       // ... existing quotes ...
       
       // Add your new quote
       addQuote(
           "Your new quote text here",
           "/audio/your_file.wav",
           QUOTE_CALM,  // or other category
           3500         // duration in milliseconds
       );
   }
   ```

2. **Create** audio file and place in `data/audio/`

3. **Upload** filesystem: `pio run --target uploadfs`

4. **Recompile** and upload: `pio run --target upload`

## Troubleshooting

### "LittleFS failed to mount"
- Run: `pio run --target uploadfs`
- Check platformio.ini has: `board_build.filesystem = littlefs`

### "Audio file not found"
- Verify file is in `data/audio/` folder
- Check filename matches exactly (case-sensitive)
- Ensure you ran `uploadfs` after adding files

### "No sound playing"
- Check speaker connections (GPIO 21, 0, 14)
- Verify SD pin is HIGH (enables MAX98357A)
- Check audio file format (must be 22050Hz mono 16-bit)

### "Out of memory"
- Audio files too large (aim for <30KB each)
- Reduce duration or quality

## Quick Commands Reference

```bash
# Build
pio run

# Upload firmware
pio run --target upload

# Upload filesystem (audio files)
pio run --target uploadfs

# Monitor serial
pio device monitor

# Clean build
pio run --target clean

# Convert audio
./scripts/convert_audio.sh input.mp3 output_name
```

## Success Checklist

- [ ] Created all 18 audio files
- [ ] Converted to correct format (22050Hz, mono, 16-bit)
- [ ] Placed files in `data/audio/`
- [ ] Uploaded filesystem (`uploadfs`)
- [ ] Uploaded firmware (`upload`)
- [ ] Verified initialization in serial monitor
- [ ] Tested voice commands
- [ ] Confirmed audio playback

## Need Help?

1. Check serial monitor for error messages
2. Read `docs/AUDIO_QUOTES.md` for detailed info
3. Verify audio format: `ffprobe data/audio/your_file.wav`
4. Test individual functions in main loop

## Implementation Status

✅ **Complete:**
- Hashmap quote-to-audio mapping system
- Category-based random selection
- Voice command integration
- LittleFS filesystem support
- Documentation and tools
- Error handling and fallbacks

⏳ **TODO (by you):**
- Create 18 audio files
- Upload filesystem to device
- Physical testing on hardware
