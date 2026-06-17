# Phase 5: High Fidelity Colors - Research

## Context
Phase 5 introduces support for 16/256 ANSI colors and 24-bit True Color (RGB) output. This satisfies REQ-REND-02 and REQ-REND-03. We need to define how colors are mapped in the Encoder, how they are represented in the `.ascv` binary format, and how they are rendered efficiently in the Player.

## Findings

### Color Representation in Format (`format.hpp`)
We will add `ColorMode` enum and update `FileHeader` to store the color mode.

```cpp
enum class ColorMode : uint8_t {
    MONOCHROME = 0,
    ANSI_16 = 1,
    ANSI_256 = 2,
    RGB_24 = 3
};

#pragma pack(push, 1)
struct FileHeader {
    uint8_t  magic[4];
    uint16_t version;
    uint16_t width;
    uint16_t height;
    uint32_t frame_count;
    uint32_t fps_numerator;
    uint32_t fps_denominator;
    uint8_t  color_mode; // ascv::ColorMode
};
#pragma pack(pop)
```

Depending on `color_mode`, each frame cell is stored differently:
- `MONOCHROME`: 1 byte per cell (`char`).
- `ANSI_16`: 2 bytes per cell (`char` + `uint8_t color_index` [0-15]).
- `ANSI_256`: 2 bytes per cell (`char` + `uint8_t color_index` [0-255]).
- `RGB_24`: 4 bytes per cell (`char` + `uint8_t r` + `uint8_t g` + `uint8_t b`).

This ensures that for Monochrome video, we don't waste any space on color, preserving the 100% backward compatibility of size and format.

### Color Mapping in Encoder
1. **FFmpeg Scaling**: Change the scale format from `AV_PIX_FMT_GRAY8` to `AV_PIX_FMT_RGB24`.
2. **Character Selection**: Calculate luminance for each RGB pixel to select the appropriate ASCII character: `Y = 0.2126 * R + 0.7152 * G + 0.0722 * B` (or standard Rec. 601: `Y = 0.299*R + 0.587*G + 0.114*B`).
3. **ANSI 16 mapping**: Compare the pixel's RGB against a static array of the standard 16 ANSI colors and choose the one with the smallest Euclidean distance: $d^2 = (R - R_i)^2 + (G - G_i)^2 + (B - B_i)^2$.
4. **ANSI 256 mapping**: Map to the closest of the 256 colors (including the 6x6x6 cube and grayscale ramp). A precalculated 256-color table is mapped using Euclidean distance for optimal accuracy.
5. **True Color**: Use the raw RGB values.

### Efficiency & ANSI Overhead in Player
To prevent massive terminal rendering overhead, the Player must maintain a "current color state" during frame rendering:
- Track the current foreground color mode and value.
- Only output the ANSI escape sequence (e.g., `\x1b[38;2;R;G;Bm` or `\x1b[38;5;Im`) when the color changes.
- At the start of each frame (after cursor-home `\x1b[H`), reset the color state.
- Use a single write buffer per frame.

### Delta Encoding with Colors
For P-frames, a "changed cell" is defined as a cell where either the character OR the color has changed. If unchanged, we write `\0` for all bytes of that cell. 
For example:
- In `RGB_24` mode, if a cell is unchanged, we write `\0\0\0\0` (4 bytes).
- Our RLE compressor will compress consecutive blocks of `\0` bytes seamlessly.

## Architecture Decisions
- **CLI Options**: Add a `--color-mode` CLI argument to `ascv_encoder` (accepts `none`, `ansi16`, `ansi256`, `truecolor`).
- **Color Palettes**: A static 16/256 color lookup table will be defined in `EncoderCore.cpp`.
