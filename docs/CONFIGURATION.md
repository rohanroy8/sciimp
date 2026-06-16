# ASCV Configuration

This document outlines the configuration and parameterization options available for the ASCV toolkit, including the build system, encoder, and player.

## 1. Encoder Configuration

The ASCV Encoder (`ascv_encoder`) is configured exclusively via command-line arguments. It uses the `CLI11` library to parse parameters.

### Required Arguments

- `-i, --input <string>`
  The path to the input video file (must be a format decodable by FFmpeg).
- `-o, --output <string>`
  The path where the encoded `.ascv` binary file will be saved.
- `-W, --width <integer>`
  The maximum output terminal width in characters.
- `-H, --height <integer>`
  The maximum output terminal height in characters.

### Optional Arguments

- `--charset <string>`
  The set of ASCII characters to use for representing varying levels of luminance.
  *Default:* `" .:-=+*#%@"`

### Logging
The encoder uses `spdlog` for execution logging. It currently outputs directly to standard output with the logging level hardcoded to `info`.

## 2. Player Configuration

Currently, the ASCV Player (`ascv_player`) accepts no command-line arguments or configuration parameters (it only prints the format version and exits). 
*VERIFY: Future iterations should accept an input `.ascv` file path and possible playback controls via CLI arguments.*

## 3. Build Configuration

The project is built using CMake. Configuration relies primarily on CMake's native variables and dependency resolution via `FetchContent`.

### Core Requirements
- **CMake**: `3.25` or higher.
- **C++ Standard**: C++20 (`CMAKE_CXX_STANDARD_REQUIRED ON`).

### CMake Options and Variables
There are no custom `option()` flags defined in `CMakeLists.txt`. Standard CMake variables apply:
- `CMAKE_BUILD_TYPE`: Standard CMake build types (e.g., `Release`, `Debug`).
- `BUILD_TESTING`: Implicitly enabled via `enable_testing()`, which adds the Catch2 test suite (e.g., `test_aspect_ratio`).

### Dependencies

Dependencies are automatically fetched and built with hardcoded configurations to guarantee stability:
- **Zstandard (zstd)**: Tag `v1.5.7`. Configured to build as a static library with programs and tests disabled (`ZSTD_BUILD_PROGRAMS OFF`, `ZSTD_BUILD_TESTS OFF`, `ZSTD_BUILD_SHARED OFF`, `ZSTD_BUILD_STATIC ON`).
- **CLI11**: Tag `v2.4.2` (Header-only).
- **spdlog**: Tag `v1.14.1`.
- **MiniAudio**: Tag `0.11.21` (Header-only).
- **Catch2**: Tag `v3.7.1` (Test framework).

### System Requirements
- **FFmpeg**: Must be pre-installed on the system and resolvable via `pkg-config` (`libavcodec`, `libavformat`, `libavutil`, `libswscale`). *VERIFY: Assuming FFmpeg 8.1+ as noted in project recommendations, though not enforced by the CMake script.*

## 4. Environment Variables

The ASCV applications do not currently read or depend on any custom environment variables.
