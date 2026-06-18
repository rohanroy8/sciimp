# ASCV (ASCII Video Format)

A proprietary, high-performance binary video format (`.ascv`) and rendering engine designed specifically for terminal-based ASCII playback. 

ASCV offloads heavy processing like video decoding and pixel-to-ASCII / color mapping math to a one-time offline **Encoding** phase, producing a highly compressed binary file. A specialized terminal **Player** can then render this file with virtually zero CPU load.

## Core Value

Achieve high-fidelity, synchronized ASCII video and audio playback in the terminal with virtually zero CPU load during playback.

## Component Overview

The project consists of two main C++ components:

### 1. The C++ Encoder (`src/encoder/`)
- **Video Decoding:** Uses FFmpeg (`libavcodec`, `libavformat`) to decode standard video files (e.g. `.mp4`).
- **Processing:** Downsamples pixels and maps luminance values to a customizable ASCII character set and ANSI color palettes (Monochrome, 16-color, 256-color, or 24-bit True Color RGB).
- **Compression:** Applies Run-Length Encoding (RLE) to color runs, calculates delta P-frames relative to key I-frames, and compresses the final blocks using **Zstandard (zstd)**.

### 2. The Terminal Player (`src/player/`)
- **Execution:** A bare-metal, high-performance C++ executable utilizing raw terminal mode and zero-heap-allocation render loops to guarantee minimal CPU overhead.
- **Index Generation:** Sequentially scans the `.ascv` file on startup to map the file offsets and types of all frames, enabling instant $O(1)$ seeking.
- **Rendering:** Implements single-buffer ANSI escape flushes directly using POSIX `write` or Windows console writes to eliminate screen tearing.
- **Audio Sync (Master Clock):** Integrates **MiniAudio** to play a sidecar `.wav` track, using the audio device sound cursor as the master clock to drive video frame pacing.
- **Interactive Controls (Hotkeys):** Non-blocking input handling allowing pause, resume, forward/backward seeking, and quit capabilities. (Disabled when reading from `stdin`).

## Technology Stack

- **Language:** C++20
- **Build System:** CMake (3.25+)
- **Video Library:** FFmpeg (libavcodec/libavformat 8.1+)
- **Compression:** Zstandard (zstd 1.5.7+)
- **Audio API:** MiniAudio (0.11+)
- **CLI Parsing:** CLI11 (2.4+)
- **Logging:** spdlog (1.14+)
- **Testing:** Catch2 (v3)

## Building the Project

### Prerequisites
Ensure you have a modern C++ compiler, CMake, Ninja, and FFmpeg installed on your system.

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config \
                 libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
```

### Build Steps

```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
```

This will build the `ascv_encoder` and `ascv_player` executables.

## Usage

### 1. Encoding a Video
Convert a standard video into an `.ascv` file:
```bash
./build/src/encoder/ascv_encoder -i input.mp4 -o output.ascv -W 80 -H 24 --color rgb24
```

### 2. Playing a Video
Render the ASCII video inside your terminal:
```bash
./build/src/player/ascv_player output.ascv
```
*(If an audio file `output.ascv.wav` exists in the same directory, the player will automatically detect it and play audio synchronized with the video).*

### Interactive Hotkeys:
- **`Spacebar`**: Toggle pause / resume.
- **`Right Arrow` / `d` / `l`**: Seek forward 5 seconds.
- **`Left Arrow` / `a` / `h`**: Seek backward 5 seconds (seeks nearest keyframe, applies deltas, and realigns audio cursor).
- **`q`**: Quit playback.

## Binary File Structure (`.ascv`)

### Global Header (23 Bytes)
- **Bytes 0-3 (Magic Number):** `ASCV` (`0x41 0x53 0x43 0x56`)
- **Bytes 4-5 (Version):** Format version (Unsigned 16-bit integer)
- **Bytes 6-7 (Width):** Target terminal columns (Unsigned 16-bit integer)
- **Bytes 8-9 (Height):** Target terminal rows (Unsigned 16-bit integer)
- **Bytes 10-13 (Total Frames):** Unsigned 32-bit integer
- **Bytes 14-17 (FPS Numerator):** Unsigned 32-bit integer
- **Bytes 18-21 (FPS Denominator):** Unsigned 32-bit integer
- **Byte 22 (Color Mode):** `0` = Monochrome, `1` = ANSI 16, `2` = ANSI 256, `3` = RGB 24-bit True Color

### Frame Blocks
Continuous sequence of frame payloads:
- **Frame Header (5 Bytes):**
  - **Byte 0 (Type):** `0` = `I_FRAME` (Keyframe), `1` = `P_FRAME` (Delta frame)
  - **Bytes 1-4 (Compressed Size):** Payload size (Unsigned 32-bit integer)
- **Payload:** Zstandard-compressed RLE color run data.

## Testing

To run the integration and unit tests:
```bash
# Test the encoder and unit tests
bash tests/test_encoder.sh

# Test player stdout pacing
bash tests/test_player.sh

# Test audio/video clock sync
bash tests/test_audio.sh
```

## Detailed Documentation
Check the `docs/` directory for full specifications:
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): Data flow and component architecture.
- [docs/CONFIGURATION.md](docs/CONFIGURATION.md): Complete lists of CLI arguments and hotkeys.
- [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md): Build options, dependencies, and style guides.
- [docs/TESTING.md](docs/TESTING.md): Testing strategies, Catch2 integrations, and shell verification runs.
