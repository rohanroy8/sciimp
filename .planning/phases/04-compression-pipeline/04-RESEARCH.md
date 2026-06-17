# Phase 4: Compression Pipeline - Research

## Context
Phase 4 focuses on integrating RLE, zstd compression, and delta frames into the Encoder and Player. This will drastically reduce `.ascv` file sizes and enable fast rendering, fulfilling REQ-COMP-03 and REQ-COMP-05.

## Findings

### Format Additions (`format.hpp`)
We need to introduce a `FrameHeader` to support varying frame sizes (due to compression) and frame types (I-frame vs P-frame).

```cpp
enum class FrameType : uint8_t {
    I_FRAME = 0, // Keyframe (Full frame data)
    P_FRAME = 1  // Delta frame
};

#pragma pack(push, 1)
struct FrameHeader {
    FrameType type;
    uint32_t compressed_size; // Size of the zstd-compressed RLE payload
};
#pragma pack(pop)
```

### Compression Layers
1. **Delta Frames (P-Frames)**: Instead of storing the full frame, compute the difference from the previous frame. For terminals, we can encode sparse updates. A simple, highly compressible delta scheme is to represent unchanged characters as `\0`. Thus, an I-frame is a full frame buffer of ASCII characters, and a P-frame is a buffer where unchanged characters are `\0` and changed characters are their new ASCII values.
2. **Run-Length Encoding (RLE)**: To satisfy the explicit "RLE + zstd layering" requirement, we apply a fast byte-aligned RLE algorithm before zstd. A PackBits-style approach works well: 
   - A control byte indicates a run or raw bytes.
   - For example, if we want simplicity: `[count_8bit][byte_value_8bit]`. If the frame has many `\0` (from the delta phase), RLE compresses it down to almost nothing before Zstd even touches it.
3. **Zstandard (zstd)**: The industry standard for fast decompression. We need to link `libzstd` to both the encoder and player executables in CMake.

### Encoder Implementation
- Introduce a GOP (Group of Pictures) size, e.g., 30 frames. Every 30th frame is an I-frame.
- Keep the previous frame's ASCII buffer in memory.
- For P-frames, diff against the previous buffer. Emit `\0` for unchanged characters.
- Apply RLE to the diff buffer (or I-frame buffer).
- Apply Zstd to the RLE output.
- Write `FrameHeader` + Zstd compressed data to the `.ascv` file.

### Player Implementation
- Read `FrameHeader`.
- Read `compressed_size` bytes into a compressed buffer.
- Decompress Zstd -> RLE data.
- Decompress RLE -> diff buffer (or I-frame buffer).
- If I-frame, copy to the current active frame buffer.
- If P-frame, apply the diff buffer to the active frame buffer (where diff byte != `\0`).
- Write the active frame buffer to stdout.

## Architecture Decisions
- **Delta encoding strategy**: Use `\0` to represent "no change" in P-frames. It's computationally trivial (just a byte-wise comparison and mask) and compresses massively under RLE+Zstd.
- **Dependencies**: Use `find_package(zstd REQUIRED)` or `pkg_check_modules(ZSTD REQUIRED libzstd)` in CMake. We will add a hint to `PROJECT.md` or `.github/workflows` later if CI is needed.
