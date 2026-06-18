#!/bin/bash
set -e
set -o pipefail

INPUT_VID="/tmp/ascv_test_input.mp4"
OUTPUT_ASCV="/tmp/ascv_test_output.ascv"
PLAYER_OUT="/tmp/ascv_test_player_out.txt"

echo "Generating test video..."
ffmpeg -y -f lavfi -i testsrc=duration=1:size=320x240:rate=10 -c:v libx264 -pix_fmt yuv420p "$INPUT_VID" 2>/dev/null

echo "Encoding to .ascv..."
./build/src/encoder/ascv_encoder -i "$INPUT_VID" -o "$OUTPUT_ASCV" -W 80 -H 24

echo "Running player..."
./build/src/player/ascv_player "$OUTPUT_ASCV" > "$PLAYER_OUT"

echo "Validating player output..."

# Assert output starts with ESC[H (cursor home)
# Read first 3 bytes as hex using od (universally available)
FIRST_BYTES=$(od -An -N3 -tx1 "$PLAYER_OUT" | tr -d ' \n')
EXPECTED="1b5b3f"  # \x1b[? (for \x1b[?2026h)
if [ "$FIRST_BYTES" != "$EXPECTED" ]; then
    echo "ERROR: Output does not start with \\x1b[? (ESC[?)."
    echo "  Got (hex): $FIRST_BYTES"
    echo "  Expected:  $EXPECTED"
    exit 1
fi

# Assert output file size is greater than 0
FILE_SIZE=$(wc -c < "$PLAYER_OUT")
if [ "$FILE_SIZE" -le 0 ]; then
    echo "ERROR: Player output file is empty."
    exit 1
fi

echo "All checks passed. Player output size: $FILE_SIZE bytes."
exit 0
