# Phase 1: Foundation & Build System - Research

## Objective
Answer: "What do I need to know to PLAN this phase well?"

## 1. Architectural & Technology Requirements
- **Language**: C++20 (provides `std::span` and concepts for safe binary manipulation).
- **Build System**: CMake (version 3.25+ required).
- **Compiler Flags**: Moderate strictness: `-Wall -Wextra` (Do not use `-Werror` per decision D-04).

## 2. Directory Structure
The workspace should follow a monorepo-style layout (Decision D-01). The following structure must be created:
```text
├── CMakeLists.txt              # Root CMake configuration
├── include/
│   └── ascv/                   # Shared headers
│       └── format.hpp          # .ascv binary format specification
├── src/
│   ├── encoder/                # Encoder application
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   └── player/                 # Player application
│       ├── CMakeLists.txt
│       └── main.cpp
```

## 3. Dependency Management (Decision D-02)
- **Zstandard (zstd)**: Use CMake `FetchContent` to download and link (version 1.5.7+ recommended).
- **MiniAudio**: Use CMake `FetchContent` to download the header (version 0.11+ recommended).
- **FFmpeg (libavcodec/libavformat/libavutil)**: Rely on system packages (version 8.1+ recommended). Best integrated into CMake using `find_package(PkgConfig REQUIRED)` and `pkg_check_modules`.

## 4. Binary Format Specification (`.ascv`)
The core format specification should be defined in `include/ascv/format.hpp`.
- **Magic Bytes**: `"ASCV"` (Decision D-03).
- **Endianness**: Little-endian structure. Since this is native to most modern CPUs, the specification can involve simple structs but should enforce type widths (e.g., `uint32_t`) and standard layouts.

## 5. Actionable Steps for the PLAN
When creating the execution plan (`01-PLAN.md`), include steps to:
1. **Initialize Root CMakeLists.txt**: Set C++20, moderate warning flags, and configure `FetchContent` for zstd/miniaudio and `PkgConfig` for FFmpeg.
2. **Create Common Definitions**: Create `include/ascv/format.hpp` defining magic bytes and initial basic structures.
3. **Setup Encoder Skeleton**: Create `src/encoder/CMakeLists.txt` (link FFmpeg + zstd + shared includes) and a boilerplate `main.cpp`.
4. **Setup Player Skeleton**: Create `src/player/CMakeLists.txt` (link miniaudio + zstd + shared includes) and a boilerplate `main.cpp`.

## Validation Architecture
- Verify that CMake generates the build files correctly.
- Verify that both targets (Encoder and Player) can be built using `cmake --build`.
