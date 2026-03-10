#!/bin/bash
# Audio preparation script for M.I.N.D. Companion
# Converts audio files to the correct format for ESP32 playback

SAMPLE_RATE=22050
CHANNELS=1
BIT_DEPTH=16

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================"
echo "  M.I.N.D. Audio Converter"
echo "  Target: ${SAMPLE_RATE}Hz, ${CHANNELS}ch, ${BIT_DEPTH}bit"
echo "========================================"
echo

# Check if ffmpeg is installed
if ! command -v ffmpeg &> /dev/null; then
    echo -e "${RED}ERROR: ffmpeg is not installed${NC}"
    echo "Please install ffmpeg:"
    echo "  Ubuntu/Debian: sudo apt install ffmpeg"
    echo "  macOS: brew install ffmpeg"
    echo "  Windows: Download from https://ffmpeg.org"
    exit 1
fi

# Check for input files
if [ $# -eq 0 ]; then
    echo "Usage: $0 <input_file> [output_name]"
    echo
    echo "Examples:"
    echo "  $0 my_audio.mp3"
    echo "  $0 calm_quote.wav relax"
    echo
    exit 1
fi

INPUT_FILE="$1"
OUTPUT_NAME="${2:-$(basename "$INPUT_FILE" | sed 's/\.[^.]*$//')}"
OUTPUT_FILE="${OUTPUT_NAME}.wav"

# Check if input file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo -e "${RED}ERROR: Input file not found: $INPUT_FILE${NC}"
    exit 1
fi

echo -e "${YELLOW}Input:${NC}  $INPUT_FILE"
echo -e "${YELLOW}Output:${NC} $OUTPUT_FILE"
echo

# Get input file info
echo "Input file info:"
ffprobe -v error -show_entries format=duration,bit_rate,size -of default=noprint_wrappers=1:nokey=1 "$INPUT_FILE" 2>/dev/null | while read line; do
    echo "  $line"
done
echo

# Convert audio
echo "Converting..."
ffmpeg -i "$INPUT_FILE" \
    -ar "$SAMPLE_RATE" \
    -ac "$CHANNELS" \
    -sample_fmt s16 \
    -acodec pcm_s16le \
    -y \
    "$OUTPUT_FILE" 2>&1 | grep -E "Duration|Stream|Output"

if [ $? -eq 0 ]; then
    echo
    echo -e "${GREEN}✓ Conversion successful!${NC}"
    echo
    
    # Get output file size
    SIZE=$(du -h "$OUTPUT_FILE" | cut -f1)
    DURATION=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$OUTPUT_FILE" 2>/dev/null)
    DURATION_MS=$(echo "$DURATION * 1000 / 1" | bc)
    
    echo "Output file: $OUTPUT_FILE"
    echo "  Size: $SIZE"
    echo "  Duration: ${DURATION}s (${DURATION_MS}ms)"
    echo
    echo -e "${YELLOW}Next steps:${NC}"
    echo "  1. Copy $OUTPUT_FILE to data/audio/"
    echo "  2. Add quote to audio_quotes.cpp:"
    echo "     addQuote(\"Your quote text\", \"/audio/$OUTPUT_FILE\", QUOTE_CATEGORY, $DURATION_MS);"
    echo "  3. Upload filesystem: pio run --target uploadfs"
else
    echo
    echo -e "${RED}✗ Conversion failed${NC}"
    exit 1
fi
