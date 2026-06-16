# ASCV Architecture

## 1. System Overview

ASCV (ASCII Video Format) is divided into two distinct components to satisfy the core constraint of "virtually zero CPU load during playback":
1. **The Encoder (Generator):** A heavy, one-time offline process that decodes standard video formats, performs the expensive pixel-to-ASCII and color mapping math, applies compression, and produces a proprietary `.ascv` binary.
2. **The Player:** A lightweight, bare-metal terminal application that reads the pre-computed `.ascv` file into memory, decompresses it, and streams it to the terminal using raw ANSI sequences synchronized to an audio master clock.

## 2. Component Architecture

### 2.1 The Encoder (`src/encoder/`)
**Role:** Ingest standard multimedia files and transcode them into the highly optimized `.ascv` format.

**Pipeline:**
1. **Video/Audio Demuxing & Decoding:** Uses **FFmpeg (`libavformat`, `libavcodec`)** to extract raw pixel frames and audio samples from standard containers (e.g., MP4). Bypasses high-level wrappers like OpenCV to minimize the dependency footprint.
2. **ASCII & Color Mapping:** Downsamples raw RGB/Grayscale pixel values to the target terminal resolution, evaluating and assigning them to a fixed ASCII character set and optimal ANSI 24-bit or 256-color codes.
3. **Data Compression:**
   - **Intra-frame (RLE):** The encoder writes a flat 2D uncompressed character buffer per frame.
   - **Inter-frame (Delta):** The encoder writes every frame fully as a flat uncompressed buffer.
   - **Block Compression:** The encoder writes uncompressed frame buffers directly to the output file without Zstandard.
4. **Serialization:** Packages the processed data with headers and Presentation Timestamps (PTS).

### 2.2 The Player (`src/player/`)
**Role:** Efficient terminal rendering and robust audio playback.

**Pipeline:**
1. **Deserialization & Decompression:** The player only has a stub `main.cpp` and no decompression logic.
2. **Terminal Abstraction & Setup:**
   - The player only has a stub `main.cpp` without `termios` configuration for Unix/Linux/macOS.
   - The player only has a stub `main.cpp` without `SetConsoleMode` usage for Windows.
3. **Rendering Engine (The Zero-CPU Goal):**
   - The player uses `std::cout` to print version info in `main.cpp`.
   - The player has no rendering loop or `write(STDOUT_FILENO, buffer, size)` system call.
4. **Audio Sync (Master Clock):** MiniAudio is linked but not implemented or used in the player source.

## 3. Data Architecture (`.ascv` Format)

The binary layout is strictly defined in `include/ascv/format.hpp` and prioritizes direct memory mapping and ultra-fast parsing over human readability.

### 3.1 Global Header (16+ Bytes)
A fixed-size little-endian header defining playback rules:
- `Magic (4 bytes)`: "ASCV"
- `Version (2 bytes)`
- `Resolution (4 bytes)`: Width (16-bit) and Height (16-bit)
- `Timing (12 bytes)`: FPS Numerator, FPS Denominator, Total Frames
- *(Expandable for color depth flags and audio metadata sizes)*

### 3.2 Frame Blocks (The Stream)
- No frame headers or types are defined in `format.hpp` or written by the encoder.
- The encoder writes every frame fully as a flat uncompressed buffer.

## 4. Dependencies & Constraints

- **C++20 & CMake:** Modern language features (like `std::span`) ensure safe parsing of untrusted binary inputs. CMake orchestrates the complex library linkings cross-platform.
- **FFmpeg 8.1+:** Strictly localized to the Encoder. Provides robust standard video decoding without the GUI overhead.
- **Zstandard (zstd) 1.5.7+:** Chosen for its exceptional decompression speed vs. ratio curve, making it perfectly suited for real-time, low-CPU playback environments.
- **MiniAudio 0.11+:** Header-only audio API minimizing the deployment footprint of the Player.
- **Prohibited Technology:**
  - *OpenCV:* Overkill and too heavy.
  - *Standard I/O Streams (`iostream`):* Unsuitable for full-screen high FPS writes.
  - *Interpreted Languages (Python/Node):* Cannot guarantee the strict deterministic memory and zero-CPU rendering constraints.
  - *Ncurses/Termbox:* Not optimal for direct single-buffer ANSI flushing. Raw `termios` is preferred.
