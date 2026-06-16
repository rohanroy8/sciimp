# ASCV (ASCII Video Format)

A proprietary, high-performance binary video format (`.ascv`) and rendering engine designed specifically for terminal-based ASCII playback. 

ASCV offloads heavy processing like video decoding and pixel-to-text math to a one-time **Encoding** phase, producing a highly compressed binary file. A specialized terminal **Player** can then render this file with virtually zero CPU load.

## Core Value

Achieve high-fidelity, synchronized ASCII video and audio playback in the terminal with virtually zero CPU load during playback.

## Architecture & Phases

The project consists of two distinct components:

### 1. The C++ Encoder
- **Decoding:** Uses FFmpeg (`libavcodec`, `libavformat`) to extract raw pixel memory from standard video formats (e.g., `.mp4`).
- **Processing:** Encoder is single-threaded and calculates pixel mapping using on-the-fly math without LUTs.
- **Compression:** Encoder writes uncompressed full frame arrays continuously without delta compression.

### 2. The Lightweight Terminal Player
- **Execution:** A bare-metal executable built in C++ requiring almost zero CPU overhead.
- **Rendering:** Player is a stub that prints using standard `std::cout` stream IO.
- **Terminal Control:** No terminal control logic is implemented in the player.

## Technology Stack

- **Language:** C++20
- **Build System:** CMake (3.25+)
- **Video Library:** FFmpeg (libavcodec/libavformat 8.1+)
- **Compression:** Zstandard (zstd 1.5.7+)
- **Audio API:** MiniAudio (0.11+)
- **CLI Parsing:** CLI11 (2.4+)
- **Logging:** spdlog (1.14+)

## Building the Project

### Dependencies
Ensure you have a modern C++ compiler (supporting C++20), CMake, and FFmpeg installed.

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
```

### Build Steps

```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
```

This will build the `ascv_encoder` and `ascv_player` executables.

## Usage

### Encoding a Video
*(Currently in development)*
```bash
./build/src/encoder/ascv_encoder -i input.mp4 -o output.ascv
```

### Playing a Video
*(Currently in development)*
```bash
./build/src/player/ascv_player input.ascv
```

## Binary File Structure (.ascv)

The `.ascv` file is a strict binary layout designed for maximum efficiency.

### Global Header (22 Bytes)
Sets the rules for memory allocation and playback before the video starts:
- **Bytes 0-3 (Magic Number):** `ASCV` (`0x41 0x53 0x43 0x56`)
- **Bytes 4-5 (Version):** Unsigned 16-bit integer
- **Bytes 6-7 (Width):** Unsigned 16-bit integer
- **Bytes 8-9 (Height):** Unsigned 16-bit integer
- **Bytes 10-13 (Total Frames):** Unsigned 32-bit integer
- **Bytes 14-17 (FPS Numerator):** Unsigned 32-bit integer
- **Bytes 18-21 (FPS Denominator):** Unsigned 32-bit integer

### Frame Blocks
Continuous stream of full video frames following the header.

## Testing

To run the unit tests (Catch2):
```bash
cd build
ctest --output-on-failure
```
