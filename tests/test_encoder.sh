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
EXPECTED_SIZE=$((22 + 10 * 80 * 24))
if [ "$FILE_SIZE" -ne "$EXPECTED_SIZE" ]; then
    echo "ERROR: File size is $FILE_SIZE, expected exactly $EXPECTED_SIZE bytes."
    exit 1
fi

WIDTH=$(od -j 6 -N 2 -t u2 -An "$OUTPUT_ASCV" | tr -d ' ')
if [ "$WIDTH" -ne 80 ]; then
    echo "ERROR: Width is $WIDTH, expected 80."
    exit 1
fi

HEIGHT=$(od -j 8 -N 2 -t u2 -An "$OUTPUT_ASCV" | tr -d ' ')
if [ "$HEIGHT" -ne 24 ]; then
    echo "ERROR: Height is $HEIGHT, expected 24."
    exit 1
fi

FRAME_COUNT=$(od -j 10 -N 4 -t u4 -An "$OUTPUT_ASCV" | tr -d ' ')
if [ "$FRAME_COUNT" -ne 10 ]; then
    echo "ERROR: Frame count is $FRAME_COUNT, expected 10."
    exit 1
fi

FPS_NUM=$(od -j 14 -N 4 -t u4 -An "$OUTPUT_ASCV" | tr -d ' ')
if [ "$FPS_NUM" -ne 10 ]; then
    echo "ERROR: FPS numerator is $FPS_NUM, expected 10."
    exit 1
fi

# Assert all frame data bytes are printable ASCII from default charset
# Default charset: " .:-=+*#%@" (space is allowed)
BAD_CHARS=$(dd if="$OUTPUT_ASCV" bs=1 skip=22 2>/dev/null | tr -d ' .:=+*#%@-' | wc -c)
if [ "$BAD_CHARS" -ne 0 ]; then
    echo "ERROR: Found $BAD_CHARS invalid characters in frame data."
    exit 1
fi

echo "Cleaning up..."
rm -f "$INPUT_VID" "$OUTPUT_ASCV"

echo "PASS: All encoder tests passed"
