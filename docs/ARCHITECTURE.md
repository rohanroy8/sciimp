# ASCV Architecture

## 1. System Overview

ASCV (ASCII Video Format) is divided into two distinct components to satisfy the core constraint of "virtually zero CPU load during playback":
1. **The Encoder (Generator):** A heavy, one-time offline process that decodes standard video formats, performs the expensive pixel-to-ASCII and color mapping math, applies compression, and produces a proprietary `.ascv` binary.
2. **The Player:** A lightweight, bare-metal terminal application that reads the pre-computed `.ascv` file, decompresses it on the fly, and streams it to the terminal using raw ANSI sequences. Playback timing is synchronized to an audio master clock, with support for interactive pausing and seeking controls.

## 2. Component Architecture

### 2.1 The Encoder (`src/encoder/`)
**Role:** Ingest standard multimedia files and transcode them into the highly optimized `.ascv` format.

**Pipeline:**
1. **Video/Audio Demuxing & Decoding:** Uses **FFmpeg (`libavformat`, `libavcodec`)** to extract raw pixel frames and audio samples from standard containers.
2. **ASCII & Color Mapping:** Downsamples raw RGB/Grayscale pixel values to the target terminal resolution, evaluating and assigning them to a fixed ASCII character set and optimal ANSI 16/256 or 24-bit True Color RGB codes.
3. **Data Compression:**
   - **Intra-frame (RLE):** Performs Run-Length Encoding on color runs to compress homogeneous areas.
   - **Inter-frame (Delta):** Computes difference frames (P-frames) against keyframes (I-frames) to only save changing cells.
   - **Block Compression:** Compresses the resulting frame data using **Zstandard (zstd)**.
4. **Serialization:** Packages the processed data with a file header, frame headers, and writes the `.ascv` binary format.

### 2.2 The Player (`src/player/`)
**Role:** Efficient terminal rendering and robust audio playback.

**Pipeline:**
1. **Index Generation:** Sequentially scans the `.ascv` file on startup to map the file offsets and types of all frames, enabling instant $O(1)$ seeking.
2. **Terminal Abstraction & Setup:**
   - **POSIX (Linux/macOS):** Configures standard input in raw non-blocking mode (`ICANON` and `ECHO` disabled; `VMIN=0`, `VTIME=0`) and redirects terminal output to the alternate screen buffer with cursor hidden.
   - **Windows:** Configures virtual terminal processing using `SetConsoleMode` with raw non-blocking input handling via `<conio.h>`.
3. **Decompression & Delta Rendering:**
   - Sequentially reads frame blocks from the file stream.
   - Decompresses each frame on-the-fly using Zstandard and RLE.
   - Applies frame changes to a persistent frame buffer: fully copying I-frames and applying delta overlays for P-frames.
   - Employs zero-allocation optimized buffers to minimize heap fragmentation.
4. **Rendering Engine:** Flushes the persistent frame buffer to `STDOUT_FILENO` using a single buffered `write()` system call to eliminate tearing.
5. **Audio Sync (Master Clock):** Integrates **MiniAudio** to play the sidecar `.wav` track. Uses the audio sound cursor position as the master clock to pace video rendering, with a high-precision timer fallback for video-only files.
6. **Playback Controls:** Listens to keyboard events non-blockingly (`Space` for pause/resume, `Right Arrow` or `d`/`l` to seek forward 5 seconds, `Left Arrow` or `a`/`h` to seek backward 5 seconds, and `q` to quit).

## 3. Data Architecture (`.ascv` Format)

The binary layout is strictly defined in `include/ascv/format.hpp` and prioritizes direct memory mapping and ultra-fast parsing.

### 3.1 Global Header (16+ Bytes)
A fixed-size little-endian header defining playback rules:
- `Magic (4 bytes)`: "ASCV"
- `Version (2 bytes)`
- `Resolution (4 bytes)`: Width (16-bit) and Height (16-bit)
- `Timing (12 bytes)`: Frame Count, FPS Numerator, FPS Denominator
- `Color Mode (1 byte)`: Monochrome, ANSI 16, ANSI 256, or RGB 24-bit True Color

### 3.2 Frame Blocks
Each frame block consists of:
- `FrameHeader (5 bytes)`:
  - `type` (1 byte): `I_FRAME` (0) or `P_FRAME` (1)
  - `compressed_size` (4 bytes): Size of the zstd-compressed frame payload.
- `Payload`: The compressed data containing RLE color runs.

## 4. Dependencies & Constraints

- **C++20 & CMake:** Safe binary manipulation via `std::span` and standard CMake build files.
- **FFmpeg 8.1+:** Strictly localized to the Encoder. Provides video decoding without GUI or ML dependencies.
- **Zstandard (zstd) 1.5.7+:** High-performance, fast real-time decompression.
- **MiniAudio 0.11+:** Header-only audio framework for cross-platform audio playback.
- **Raw ANSI & `termios`:** Raw escape codes for direct, high-performance terminal output. Avoids heavy window management frameworks like ncurses or termbox.
