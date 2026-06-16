#!/bin/bash
set -e
set -o pipefail

INPUT_VID="/tmp/ascv_test_input.mp4"
OUTPUT_ASCV="/tmp/ascv_test_output.ascv"

echo "Generating test video..."
ffmpeg -y -f lavfi -i testsrc=duration=1:size=320x240:rate=10 -c:v libx264 -pix_fmt yuv420p "$INPUT_VID" 2>/dev/null

echo "Running encoder..."
./build/src/encoder/ascv_encoder -i "$INPUT_VID" -o "$OUTPUT_ASCV" -W 80 -H 24

echo "Validating output..."
if [ ! -f "$OUTPUT_ASCV" ]; then
    echo "ERROR: Output file $OUTPUT_ASCV not found."
    exit 1
fi

MAGIC=$(hexdump -n 4 -e '4/1 "%02X"' "$OUTPUT_ASCV")
if [ "$MAGIC" != "41534356" ]; then
    echo "ERROR: Invalid magic bytes: $MAGIC (expected 41534356 for ASCV)"
    exit 1
fi

FILE_SIZE=$(wc -c < "$OUTPUT_ASCV")
if [ "$FILE_SIZE" -lt 22 ]; then
    echo "ERROR: File size is $FILE_SIZE, expected at least 22 bytes."
    exit 1
fi

echo "Cleaning up..."
rm -f "$INPUT_VID" "$OUTPUT_ASCV"

echo "PASS: All encoder tests passed"
