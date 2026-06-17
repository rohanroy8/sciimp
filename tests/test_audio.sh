#!/bin/bash
set -e
set -o pipefail

INPUT_VID="/tmp/ascv_audio_test_input.mp4"
OUTPUT_ASCV="/tmp/ascv_audio_test_output.ascv"
OUTPUT_WAV="/tmp/ascv_audio_test_output.ascv.wav"
PLAYER_OUT="/tmp/ascv_audio_test_player_out.txt"

echo "Generating test video with audio (sine wave + testsrc)..."
ffmpeg -y -f lavfi -i testsrc=duration=2:size=320x240:rate=10 -f lavfi -i sine=frequency=440:duration=2 -c:v libx264 -c:a aac -pix_fmt yuv420p "$INPUT_VID" 2>/dev/null

echo "Running encoder..."
./build/src/encoder/ascv_encoder -i "$INPUT_VID" -o "$OUTPUT_ASCV" -W 80 -H 24

echo "Checking output files..."
if [ ! -f "$OUTPUT_ASCV" ]; then
    echo "ERROR: Output file $OUTPUT_ASCV not found."
    exit 1
fi

if [ ! -f "$OUTPUT_WAV" ]; then
    echo "ERROR: WAV sidecar file $OUTPUT_WAV not found."
    exit 1
fi

echo "Validating WAV file header..."
# Check RIFF header (bytes 0-4)
WAV_MAGIC=$(hexdump -n 4 -e '4/1 "%02X"' "$OUTPUT_WAV")
if [ "$WAV_MAGIC" != "52494646" ]; then # RIFF
    echo "ERROR: Invalid WAV magic: $WAV_MAGIC (expected 52494646 for RIFF)"
    exit 1
fi

# Check WAVE fmt (bytes 8-12)
WAVE_MAGIC=$(hexdump -s 8 -n 4 -e '4/1 "%02X"' "$OUTPUT_WAV")
if [ "$WAVE_MAGIC" != "57415645" ]; then # WAVE
    echo "ERROR: Invalid WAV format: $WAVE_MAGIC (expected 57415645 for WAVE)"
    exit 1
fi

# Check audio format is 1 (PCM), channels is 2, sample rate is 44100
AUDIO_FORMAT=$(od -j 20 -N 2 -t u2 -An "$OUTPUT_WAV" | tr -d ' ')
CHANNELS=$(od -j 22 -N 2 -t u2 -An "$OUTPUT_WAV" | tr -d ' ')
SAMPLE_RATE=$(od -j 24 -N 4 -t u4 -An "$OUTPUT_WAV" | tr -d ' ')

if [ "$AUDIO_FORMAT" -ne 1 ]; then
    echo "ERROR: Expected PCM format (1), got $AUDIO_FORMAT."
    exit 1
fi

if [ "$CHANNELS" -ne 2 ]; then
    echo "ERROR: Expected 2 channels (stereo), got $CHANNELS."
    exit 1
fi

if [ "$SAMPLE_RATE" -ne 44100 ]; then
    echo "ERROR: Expected 44100Hz sample rate, got $SAMPLE_RATE."
    exit 1
fi

echo "WAV validation passed. Size: $(wc -c < "$OUTPUT_WAV") bytes."

echo "Running player with WAV sidecar..."
./build/src/player/ascv_player "$OUTPUT_ASCV" > "$PLAYER_OUT"

echo "Validating player output..."
FIRST_BYTES=$(od -An -N3 -tx1 "$PLAYER_OUT" | tr -d ' \n')
EXPECTED="1b5b48" # \x1b[H
if [ "$FIRST_BYTES" != "$EXPECTED" ]; then
    echo "ERROR: Output does not start with \\x1b[H (ESC[H)."
    echo "  Got (hex): $FIRST_BYTES"
    echo "  Expected:  $EXPECTED"
    exit 1
fi

echo "Cleaning up..."
rm -f "$INPUT_VID" "$OUTPUT_ASCV" "$OUTPUT_WAV" "$PLAYER_OUT"

echo "PASS: Audio tests passed successfully!"
exit 0
