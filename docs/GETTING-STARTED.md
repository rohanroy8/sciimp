# Getting Started with ASCV

Welcome to the ASCV (ASCII Video Format) project! This guide will walk you through setting up your development environment, building the project, and using the tools provided by the ASCV engine.

## Prerequisites

To build and run ASCV, you'll need the following tools and libraries:

- **C++ Compiler**: A compiler supporting C++20 (e.g., GCC 11+, Clang 14+, or MSVC).
- **CMake**: Version 3.25 or newer.
- **Ninja**: Highly recommended as the build backend for CMake.
- **FFmpeg**: Required for decoding video files in the encoder (`libavcodec`, `libavformat`, `libavutil`, `libswscale` versions 8.1+).

*(Note: Other libraries such as MiniAudio, Zstandard, CLI11, and spdlog are fetched automatically by CMake via FetchContent).*

## Environment Setup

### Ubuntu / Debian

Install the core dependencies using `apt`:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config \
                 libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
```

### macOS

Using [Homebrew](https://brew.sh/):

```bash
brew update
brew install cmake ninja pkg-config ffmpeg
```

### Windows

On Windows, we recommend using [vcpkg](https://vcpkg.io/):

```cmd
vcpkg install ffmpeg:x64-windows
```
*(Ensure you have Visual Studio with C++ desktop development workload installed).*

## Building the Project

ASCV uses CMake for cross-platform builds. We recommend using Ninja as the generator for the fastest build times.

1. **Clone the repository:**
   ```bash
   git clone <repository-url> ASCV
   cd ASCV
   ```

2. **Configure the project:**
   Create a build directory and configure CMake:
   ```bash
   mkdir build && cd build
   cmake -G Ninja ..
   ```

3. **Build the executables:**
   ```bash
   ninja
   ```
   This process will compile both the `ascv_encoder` and `ascv_player` binaries.

## Running the Tools

ASCV consists of two main tools: the Encoder and the Player.

### 1. Encoding a Video

The Encoder takes a standard video file (like `.mp4`) and converts it into the compressed `.ascv` binary format.

**Command:**
```bash
./src/encoder/ascv_encoder -i path/to/input.mp4 -o path/to/output.ascv -W 80 -H 24 --color rgb24
```

### 2. Playing a Video

The Player reads the `.ascv` file, decompresses it, and renders the ASCII frames directly to your terminal.

**Command:**
```bash
./src/player/ascv_player path/to/output.ascv
```

If a `.wav` file with the same base name (e.g. `output.ascv.wav`) exists in the same directory, the player automatically plays synchronized audio.

#### Playback Controls (Hotkeys):
- **`Spacebar`**: Pause / Resume.
- **`Right Arrow` / `d` / `l`**: Seek forward 5 seconds.
- **`Left Arrow` / `a` / `h`**: Seek backward 5 seconds.
- **`q`**: Quit playback.

## Running Tests

To run the integration and unit tests, execute the test scripts from the repository root:

```bash
# Test the encoder, unit tests, and aspect ratio padding
bash tests/test_encoder.sh

# Test the player rendering and timing fallback
bash tests/test_player.sh

# Test the audio sync and sidecar playback
bash tests/test_audio.sh
```

## Next Steps

- **Read the Architecture Document:** Check out `docs/ARCHITECTURE.md` to understand the data flow, the binary format structure, and how the zero-CPU-load player works.
- **Review Configurations:** Look at `docs/CONFIGURATION.md` to see advanced tuning options for encoding quality, color modes, and terminal compatibility.
- **Explore the Codebase:** The core logic is split between `src/encoder/` and `src/player/`. Start there to see how FFmpeg, Zstandard, MiniAudio, and raw terminal rendering are integrated.
