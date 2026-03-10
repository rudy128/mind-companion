# Audio Quotes System

## Overview
The Audio Quotes system provides a structured way to map motivational quotes and prompts to pre-recorded audio files stored in LittleFS. This enables the M.I.N.D. companion to play appropriate audio responses based on user needs and voice commands.

## Architecture

### Components
- **`audio_quotes.h/cpp`**: Core quote management system
- **`command_handler.cpp`**: Integrates audio quotes with voice commands
- **`speaker.h/cpp`**: Low-level audio playback via I2S MAX98357A

### Quote Categories
```cpp
enum QuoteCategory {
    QUOTE_CALM,         // Calming/relaxation quotes
    QUOTE_MOTIVATE,     // Motivational quotes
    QUOTE_BREATHE,      // Breathing exercise prompts
    QUOTE_EMERGENCY,    // Emergency situation quotes
    QUOTE_AWAKE,        // Keep awake prompts
    QUOTE_GREETING      // Greetings
};
```

## Usage

### Initialization
```cpp
void setup() {
    speakerInit();        // Initialize I2S speaker first
    audioQuotesInit();    // Initialize quote system
    listAllQuotes();      // Optional: debug listing
}
```

### Playing Quotes

#### By Category (Random)
```cpp
// Play a random calming quote
playQuoteByCategory(QUOTE_CALM);

// Play a random breathing exercise
playQuoteByCategory(QUOTE_BREATHE);
```

#### By Text Search
```cpp
// Find and play a specific quote
const AudioQuote* quote = getQuoteByText("relax");
if (quote) {
    playQuote(quote);
}
```

#### Direct Access
```cpp
// Get a random quote from category
const AudioQuote* quote = getRandomQuote(QUOTE_MOTIVATE);
if (quote) {
    Serial.println(quote->text);
    Serial.println(quote->audioFile);
    playQuote(quote);
}
```

## Audio File Format

### Requirements
- **Format**: WAV or raw PCM
- **Sample Rate**: 22050 Hz (SPK_SAMPLE_RATE)
- **Bit Depth**: 16-bit
- **Channels**: Mono
- **Endianness**: Little-endian

### File Storage
Audio files must be stored in LittleFS under the `/audio/` directory:
```
/audio/relax.wav
/audio/breathe_together.wav
/audio/emergency.wav
...
```

### Adding New Quotes

#### 1. Add Quote to Database
Edit `src/actuators/audio_quotes.cpp` in the `audioQuotesInit()` function:

```cpp
void audioQuotesInit() {
    // ... existing code ...
    
    // Add your new quote
    addQuote(
        "Your quote text here",           // Quote text
        "/audio/your_file.wav",          // Audio file path
        QUOTE_CALM,                      // Category
        3500                             // Duration in milliseconds
    );
}
```

#### 2. Prepare Audio File
Convert your audio to the correct format using FFmpeg:

```bash
ffmpeg -i input.mp3 -ar 22050 -ac 1 -sample_fmt s16 output.wav
```

#### 3. Upload to LittleFS
Place your audio file in the `data/audio/` folder, then upload:

```bash
pio run --target uploadfs
```

## Voice Command Integration

The system automatically maps voice commands to quote categories:

| Voice Command | Quote Category | Example Quotes |
|--------------|---------------|----------------|
| "breathing" / "breathe" | `QUOTE_BREATHE` | "Deep breath in..." |
| "how am i" / "status" | `QUOTE_CALM` | "Relax. You got this." |
| "help me" | `QUOTE_CALM` | "I'm here with you." |
| "emergency" / "panic" | `QUOTE_EMERGENCY` | "Emergency support activated." |

### Example Flow
1. User says: "I need help breathing"
2. Speech recognition → OpenAI transcription
3. `commandParse()` → `CMD_BREATHING_PATTERN`
4. `commandExecute()` → `playQuoteByCategory(QUOTE_BREATHE)`
5. Random breathing quote plays via speaker

## API Reference

### Core Functions

#### `void audioQuotesInit()`
Initialize the audio quotes system. Call once in `setup()`.

#### `const AudioQuote* getRandomQuote(QuoteCategory category)`
Returns a random quote from the specified category, or `nullptr` if none found.

#### `const AudioQuote* getQuoteByText(const char* searchText)`
Find a quote by partial text match (case-insensitive).

#### `bool playQuote(const AudioQuote* quote)`
Play a specific quote. Returns `true` on success, `false` on error.

#### `bool playQuoteByCategory(QuoteCategory category)`
Play a random quote from the specified category. Returns `true` on success.

#### `bool audioFileExists(const char* filepath)`
Check if an audio file exists in LittleFS.

#### `void listAllQuotes()`
Print all quotes to serial log (debugging).

## Troubleshooting

### Quote Not Playing
1. Check serial output for error messages
2. Verify audio file exists: `audioFileExists("/audio/file.wav")`
3. Confirm correct sample rate (22050 Hz)
4. Check LittleFS mounted successfully
5. Ensure speaker initialized: `speakerInit()`

### Missing Audio File
```
[ERROR] AQ: Audio file missing: /audio/relax.wav - falling back to text display
```
**Solution**: Upload filesystem image with `pio run --target uploadfs`

### Out of Memory
```
[ERROR] AQ: Failed to allocate 65536 bytes for audio buffer
```
**Solution**: Audio files too large. Reduce duration or quality.

### I2S Conflicts
The speaker uses I2S port 1. Only one audio source can play at a time:
- TTS playback
- Audio quotes
- Alarm/buzzer tones

The system uses `speakerI2SMutex` to prevent conflicts.

## Example Quotes Database

```cpp
// Calm quotes
addQuote("Relax. Slow down. You got this.", "/audio/relax.wav", QUOTE_CALM, 3000);
addQuote("Take it easy. Everything will be fine.", "/audio/take_it_easy.wav", QUOTE_CALM, 3500);

// Breathing quotes
addQuote("Let's breathe together. In... and out.", "/audio/breathe_together.wav", QUOTE_BREATHE, 5000);
addQuote("Deep breath in. Hold. Now exhale slowly.", "/audio/deep_breath.wav", QUOTE_BREATHE, 6000);

// Emergency quotes
addQuote("I'm here with you. You're safe.", "/audio/safe.wav", QUOTE_EMERGENCY, 3000);
addQuote("Emergency support activated.", "/audio/emergency.wav", QUOTE_EMERGENCY, 4000);

// Awake prompts
addQuote("Hey, stay with me. Keep your eyes open.", "/audio/stay_awake.wav", QUOTE_AWAKE, 3500);
addQuote("Time to wake up. Let's keep you alert.", "/audio/wake_up.wav", QUOTE_AWAKE, 3000);
```

## Future Enhancements

- [ ] Load quotes from JSON config file in LittleFS
- [ ] Support multiple languages
- [ ] Dynamic TTS generation with caching
- [ ] Quote playback history tracking
- [ ] User preference learning
- [ ] Playlist/sequence support for multi-quote sessions
- [ ] Web dashboard for quote management

## Dependencies

- **LittleFS**: Filesystem for audio storage
- **ESP32 I2S**: Audio output
- **Speaker Module**: Low-level I2S driver
- **Command Handler**: Voice command integration

## Configuration

In `config.h`:
```cpp
#define SPK_SAMPLE_RATE   22050   // Audio sample rate
#define SPK_I2S_PORT      I2S_NUM_1
#define SPK_I2S_BCLK      21
#define SPK_I2S_LRC       0
#define SPK_I2S_DIN       14
```
