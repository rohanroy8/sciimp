# Getting Started with ASCV

Welcome to the ASCV (ASCII Video Format) project! This guide will walk you through setting up your development environment, building the project, and using the basic tools provided by the ASCV engine.

## Prerequisites

To build and run ASCV, you'll need the following tools and libraries:

- **C++ Compiler**: A compiler supporting C++20 (e.g., GCC 10+, Clang 10+, or MSVC).
- **CMake**: Version 3.25 or newer.
- **Ninja**: Highly recommended as the build backend for CMake.
- **FFmpeg**: Required for decoding video files in the encoder (`libavcodec`, `libavformat`, `libavutil`, `libswscale` versions 8.1+).

*(Note: Other libraries such as MiniAudio, Zstandard, CLI11, and spdlog may be fetched automatically by CMake via FetchContent or require separate installation depending on the system).*

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

The Encoder takes a standard video file (like `.mp4`) and converts it into the highly compressed `.ascv` binary format.

**Command:**
```bash
./src/encoder/ascv_encoder -i path/to/input.mp4 -o path/to/output.ascv -W 80 -H 24
```

### 2. Playing a Video

Currently, the Player is a stub that prints a version string. In the future, it will read the `.ascv` file and render it directly to your terminal.

**Command:**
```bash
./src/player/ascv_player path/to/output.ascv
```

## Running Tests

To ensure everything is working correctly, you can run the project's test suite by executing the test binaries directly. 

From the `build` directory, run:
```bash
./test_aspect_ratio
```

## Next Steps

- **Read the Architecture Document:** Check out `docs/ARCHITECTURE.md` to understand the data flow, the binary format structure, and how the zero-CPU-load player works.
- **Review Configurations:** Look at `docs/CONFIGURATION.md` to see advanced tuning options for encoding quality, color maps, and terminal compatibility.
- **Explore the Codebase:** The core logic is split between `src/encoder/` and `src/player/`. Start there to see how FFmpeg and raw terminal rendering are integrated.
