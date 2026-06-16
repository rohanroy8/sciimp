# ASCV Development Guide

Welcome to the development guide for the ASCV (ASCII Video Format) project. This document outlines the setup, build process, project structure, and conventions required to contribute to the project.

## 1. Prerequisites

ASCV relies on C++20 and system-level multimedia libraries. Ensure you have the following installed before building:

### Core Build Tools
- **CMake:** Version 3.25 or newer
- **Compiler:** GCC 11+, Clang 14+, or MSVC supporting C++20
- **PkgConfig:** Required for locating system FFmpeg libraries
- **Ninja:** (Recommended) Faster build backend

### System Libraries (FFmpeg)
The Encoder requires FFmpeg (specifically `libavcodec`, `libavformat`, `libavutil`, and `libswscale`) to be installed on your system.

**Ubuntu / Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake pkg-config ninja-build
sudo apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
```

**macOS (Homebrew):**
```bash
brew install cmake pkg-config ninja ffmpeg
```

*Note: Other dependencies like `zstd`, `CLI11`, `spdlog`, `miniaudio`, and `Catch2` are fetched automatically via CMake's `FetchContent` module.*

## 2. Building the Project

We recommend performing an out-of-source build using a `build/` directory.

### Standard Build
```bash
# 1. Create and enter the build directory
mkdir build
cd build

# 2. Configure the project with CMake (Ninja recommended)
#    Use -DCMAKE_BUILD_TYPE=Release for performance testing
cmake -G Ninja ..

# 3. Compile the project
cmake --build .
```

The compiled binaries (`ascv_encoder`, `ascv_player`, and test executables) will be located within the `build/` subdirectory hierarchy.

## 3. Project Structure

- `CMakeLists.txt` - Top-level build configuration and dependency management.
- `src/encoder/` - Source code for the ASCV Encoder (`ascv_encoder`). Handles video decoding (via FFmpeg), ASCII mapping, and file generation.
- `src/player/` - Source code for the ASCV Player (`ascv_player`). Handles `.ascv` file reading, terminal rendering, and audio sync.
- `include/` - Common headers, including the `.ascv` binary format specification (`ascv/format.hpp`).
- `tests/` - Catch2 unit tests.
- `docs/` - Architecture, Configuration, and Development documentation.

## 4. Running Tests

ASCV uses [Catch2](https://github.com/catchorg/Catch2) for unit testing. Once the project is built, you can run the test suite using CTest or directly invoking the test binaries.

```bash
cd build

# Run all tests using CTest
ctest --output-on-failure

# Or run a specific test executable directly
./test_aspect_ratio
```

## 5. Coding Conventions & Style

- **Standard:** The project uses standard **C++20**. Leverage modern features like `std::span`, concepts, and `constexpr` where appropriate to ensure safety and performance, especially when parsing binary data.
- **Formatting:** A `.clang-format` file is provided in the repository root. Ensure your code is formatted before committing:
  ```bash
  clang-format -i path/to/your/file.cpp
  ```
- **Dependencies:** Keep the Player entirely free of heavy dependencies (like FFmpeg). Only the Encoder should link against large external media libraries. The Player should rely solely on the `.ascv` file and lightweight dependencies like `miniaudio`.
- **Debugging Tools:** Use tools like Valgrind or AddressSanitizer (ASan) to catch memory leaks, especially given the frame-by-frame memory processing and pointer manipulation in C++.

## 6. Development Workflow (GSD)

ASCV development utilizes an AI-assisted workflow driven by GSD (Get Shit Done). Before modifying code or documentation directly, always initiate the appropriate GSD command:

- Use `/gsd-quick` for simple fixes, refactors, and minor doc updates.
- Use `/gsd-debug` for investigating and fixing bugs.
- Use `/gsd-execute-phase` when working on planned milestone phases.

This ensures that all planning artifacts and project context remain perfectly synchronized with your execution. Avoid direct edits outside of a GSD workflow unless explicitly bypassing it.
