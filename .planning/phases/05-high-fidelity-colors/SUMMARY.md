# Phase 05: High Fidelity Colors — Summary

This phase implements robust support for 16-color ANSI, 256-color ANSI, and 24-bit True Color (RGB) rendering modes in the ASCV encoder and playback engine.

## Changes Made

### 1. Binary Format Update (`include/ascv/format.hpp`)
- Defined `ascv::ColorMode` enum:
  - `MONOCHROME = 0`
  - `ANSI_16 = 1`
  - `ANSI_256 = 2`
  - `RGB_24 = 3`
- Appended `color_mode` (1 byte) field to the packed `ascv::FileHeader` structure.
- Incremented `FORMAT_VERSION` to `2`.
- Updated symbol links: [`ColorMode`](file:///home/roy/project/ASCV/include/ascv/format.hpp#L12) and [`FileHeader`](file:///home/roy/project/ASCV/include/ascv/format.hpp#L20).

### 2. Encoder CLI Extension (`src/encoder/main.cpp`)
- Added command line option `--color-mode` using CLI11, accepting `none` (default), `ansi16`, `ansi256`, and `truecolor`.
- Validated choices via `CLI::IsMember`.
- Mapped inputs to the `ascv::ColorMode` enum and passed it to the encoding engine.

### 3. High-Fidelity Color Encoding Engine (`src/encoder/EncoderCore.cpp`)
- Updated FFmpeg scaling context target format from `AV_PIX_FMT_GRAY8` to `AV_PIX_FMT_RGB24`.
- Calculated character mapping by mapping RGB to luminance ($Y = 0.299R + 0.587G + 0.114B$).
- Implemented nearest-neighbor color quantization using Euclidean distance matching for ANSI 16 and 256 palettes.
- Structured cell outputs by mode cell-size ($S$):
  - Monochrome ($S = 1$): `[char]`
  - ANSI 16/256 ($S = 2$): `[char][color_index]`
  - True Color ($S = 4$): `[char][R][G][B]`
- Implemented cell-by-cell delta comparison (P-frame) where unchanged cells are encoded to all `\0` bytes, and changed cells are copied entirely to preserve color attributes.
- Refactored core frame processing logic to avoid code duplication between decoding loop and final decoder flush.

### 4. Color Playback Engine (`src/player/main.cpp`)
- Dynamically parsed cell size $S$ from `color_mode` header field.
- Allocated frame buffer sized `W * H * S`.
- Applied delta P-frames cell-by-cell checking if `cell[0] != '\0'` (since character is never `\0`).
- Implemented an efficient rendering loop with color state tracking (`last_fg_color`) to write ANSI color escape sequences ONLY when a color change occurs, avoiding overhead.
- Reset terminal colors (`\x1b[0m`) at the end of each frame and on exit.

## Verification

Due to automated terminal approval timeouts on the host environment, manual verification of the compilation and player rendering is recommended.

To verify, run:
```bash
# 1. Clean and build the project
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. Run unit tests
./build/test_aspect_ratio

# 3. Test video encoding and rendering in different color modes:
# Monochrome (default)
./build/src/encoder/ascv_encoder -i /path/to/video.mp4 -o test_mono.ascv -W 80 -H 24 --color-mode none
./build/src/player/ascv_player test_mono.ascv

# ANSI 16
./build/src/encoder/ascv_encoder -i /path/to/video.mp4 -o test_ansi16.ascv -W 80 -H 24 --color-mode ansi16
./build/src/player/ascv_player test_ansi16.ascv

# ANSI 256
./build/src/encoder/ascv_encoder -i /path/to/video.mp4 -o test_ansi256.ascv -W 80 -H 24 --color-mode ansi256
./build/src/player/ascv_player test_ansi256.ascv

# True Color (RGB 24-bit)
./build/src/encoder/ascv_encoder -i /path/to/video.mp4 -o test_rgb.ascv -W 80 -H 24 --color-mode truecolor
./build/src/player/ascv_player test_rgb.ascv
```
