# Audio Files Directory

This directory contains pre-recorded audio quotes for the M.I.N.D. Companion system.

## Required Files

Based on `audio_quotes.cpp`, the following audio files should be present:

### Calm/Relaxation
- `relax.wav` - "Relax. Slow down. You got this."
- `take_it_easy.wav` - "Take it easy. Everything will be fine."
- `breathe_peace.wav` - "Breathe in peace. Breathe out stress."

### Motivational
- `stronger.wav` - "You are stronger than you think."
- `keep_going.wav` - "Keep going. You're doing great."
- `believe.wav` - "Believe in yourself. You can do this."

### Breathing Exercises
- `breathe_together.wav` - "Let's breathe together. In... and out."
- `deep_breath.wav` - "Deep breath in. Hold. Now exhale slowly."
- `focus_breathing.wav` - "Focus on your breathing..."

### Emergency
- `safe.wav` - "I'm here with you. You're safe."
- `emergency.wav` - "Emergency support activated. Help is on the way."
- `monitoring.wav` - "Stay calm. I'm monitoring your vitals."

### Keep Awake
- `stay_awake.wav` - "Hey, stay with me. Keep your eyes open."
- `wake_up.wav` - "Time to wake up. Let's keep you alert."
- `drowsy.wav` - "I noticed you're getting drowsy..."

### Greetings
- `hello.wav` - "Hello! I'm M.I.N.D., your companion..."
- `good_morning.wav` - "Good morning! Ready to start your day?"
- `welcome.wav` - "Welcome back! I'm here to support you."

## File Format

All files must be:
- **Format**: WAV (PCM)
- **Sample Rate**: 22050 Hz
- **Channels**: Mono (1)
- **Bit Depth**: 16-bit signed little-endian

## Converting Audio

Use the conversion script:
```bash
cd scripts
./convert_audio.sh input_file.mp3 output_name
```

Or use FFmpeg directly:
```bash
ffmpeg -i input.mp3 -ar 22050 -ac 1 -sample_fmt s16 output.wav
```

## Uploading to ESP32

After adding audio files to this directory:
```bash
pio run --target uploadfs
```

This will create a LittleFS image and upload it to the ESP32.
