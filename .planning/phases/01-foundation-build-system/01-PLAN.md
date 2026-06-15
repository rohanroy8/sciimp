---
phase: 1
plan_id: 01
title: "Foundation & Build System"
wave: 1
depends_on: []
files_modified:
  - CMakeLists.txt
  - include/ascv/format.hpp
  - src/encoder/CMakeLists.txt
  - src/encoder/main.cpp
  - src/player/CMakeLists.txt
  - src/player/main.cpp
  - .clang-format
requirements:
  - REQ-COMP-04
autonomous: true
---

# Plan 01: Foundation & Build System

## Objective

Set up the core project structure, CMake build system with C++20 and FetchContent dependencies, define the `.ascv` binary format specification header, and create encoder/player skeleton executables that compile successfully.

## must_haves

- [ ] Root `CMakeLists.txt` configures the project with C++20, `-Wall -Wextra`, and `FetchContent` for zstd and miniaudio
- [ ] FFmpeg is discovered via `PkgConfig` in CMake
- [ ] Shared header `include/ascv/format.hpp` defines magic bytes `"ASCV"` and little-endian structs
- [ ] Encoder executable skeleton compiles and links against FFmpeg + zstd + shared includes
- [ ] Player executable skeleton compiles and links against miniaudio + zstd + shared includes
- [ ] `cmake -B build && cmake --build build` succeeds with zero errors

---

## Wave 1: Root Build System & Shared Header

### Task 01-01-01: Create Root CMakeLists.txt

<read_first>
- .planning/phases/01-foundation-build-system/01-CONTEXT.md
- .planning/phases/01-foundation-build-system/01-RESEARCH.md
</read_first>

<action>
Create `CMakeLists.txt` at the project root with:
- `cmake_minimum_required(VERSION 3.25)`
- `project(ASCV LANGUAGES CXX)`
- `set(CMAKE_CXX_STANDARD 20)` and `set(CMAKE_CXX_STANDARD_REQUIRED ON)`
- Compiler flags: `add_compile_options(-Wall -Wextra)` (no `-Werror` per D-04)
- `include(FetchContent)` block that declares:
  - zstd from GitHub `facebook/zstd` tag `v1.5.7`, with `ZSTD_BUILD_PROGRAMS OFF`, `ZSTD_BUILD_TESTS OFF`, `ZSTD_BUILD_SHARED OFF`, `ZSTD_BUILD_STATIC ON`. Use `FetchContent_MakeAvailable(zstd)`. The CMake target is `libzstd_static`.
  - miniaudio from GitHub `mackron/miniaudio` tag `0.11.21`. Since miniaudio is header-only, create an INTERFACE library target: `add_library(miniaudio INTERFACE)` with `target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR})`.
- `find_package(PkgConfig REQUIRED)` and `pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale)` for system FFmpeg.
- `add_subdirectory(src/encoder)` and `add_subdirectory(src/player)`
- `target_include_directories` on both targets pointing to `${PROJECT_SOURCE_DIR}/include`
</action>

<acceptance_criteria>
- CMakeLists.txt exists at project root
- File contains `cmake_minimum_required(VERSION 3.25)`
- File contains `set(CMAKE_CXX_STANDARD 20)`
- File contains `add_compile_options(-Wall -Wextra)` and does NOT contain `-Werror`
- File contains `FetchContent_Declare(zstd` with GitHub URL for facebook/zstd
- File contains `FetchContent_Declare(miniaudio` with GitHub URL for mackron/miniaudio
- File contains `pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale)`
- File contains `add_subdirectory(src/encoder)` and `add_subdirectory(src/player)`
</acceptance_criteria>

<automated>
cmake -B build 2>&1 | head -30
</automated>

### Task 01-01-02: Create Shared Format Header

<read_first>
- .planning/phases/01-foundation-build-system/01-CONTEXT.md (D-03 for endianness and magic bytes)
</read_first>

<action>
Create `include/ascv/format.hpp` with:
- `#pragma once`
- `#include <cstdint>` and `#include <cstring>`
- `namespace ascv` wrapping all definitions
- `constexpr uint8_t MAGIC[4] = {'A', 'S', 'C', 'V'};`
- `constexpr uint16_t FORMAT_VERSION = 1;`
- A `#pragma pack(push, 1)` / `#pragma pack(pop)` block containing:
  - `struct FileHeader` with fields: `uint8_t magic[4]`, `uint16_t version`, `uint16_t width`, `uint16_t height`, `uint32_t frame_count`, `uint32_t fps_numerator`, `uint32_t fps_denominator` — all little-endian (native on x86/ARM).
- A helper `inline bool validate_magic(const FileHeader& h)` that returns `std::memcmp(h.magic, MAGIC, 4) == 0`
- Comment at top: `// ASCV Binary Format Specification — Little-endian, see D-03`
</action>

<acceptance_criteria>
- `include/ascv/format.hpp` exists
- File contains `namespace ascv`
- File contains `constexpr uint8_t MAGIC[4] = {'A', 'S', 'C', 'V'}`
- File contains `struct FileHeader` with `uint8_t magic[4]`, `uint16_t version`, `uint16_t width`, `uint16_t height`, `uint32_t frame_count`
- File contains `validate_magic` function
- File uses `#pragma pack(push, 1)` for struct packing
</acceptance_criteria>

### Task 01-01-03: Create .clang-format

<read_first>
- (none — new file)
</read_first>

<action>
Create `.clang-format` at the project root with `BasedOnStyle: Google` and `ColumnLimit: 100`. This enforces consistent formatting across the project per the stack recommendation.
</action>

<acceptance_criteria>
- `.clang-format` exists at project root
- File contains `BasedOnStyle: Google`
</acceptance_criteria>

---

## Wave 2: Encoder & Player Skeletons

### Task 01-02-01: Create Encoder Skeleton

<read_first>
- CMakeLists.txt (root — to see FetchContent targets and include paths)
- include/ascv/format.hpp (shared header to include)
</read_first>

<action>
Create `src/encoder/CMakeLists.txt` with:
- `add_executable(ascv_encoder main.cpp)`
- `target_link_libraries(ascv_encoder PRIVATE PkgConfig::FFMPEG libzstd_static)`
- `target_include_directories(ascv_encoder PRIVATE ${PROJECT_SOURCE_DIR}/include)`

Create `src/encoder/main.cpp` with:
- `#include "ascv/format.hpp"`
- `#include <iostream>`
- Wrap FFmpeg headers in `extern "C" { #include <libavcodec/avcodec.h> #include <libavformat/avformat.h> }`
- `int main(int argc, char* argv[])` that prints `"ASCV Encoder v" << ascv::FORMAT_VERSION` and returns 0
</action>

<acceptance_criteria>
- `src/encoder/CMakeLists.txt` exists and contains `add_executable(ascv_encoder`
- `src/encoder/CMakeLists.txt` links `PkgConfig::FFMPEG` and `libzstd_static`
- `src/encoder/main.cpp` includes `ascv/format.hpp`
- `src/encoder/main.cpp` wraps FFmpeg headers in `extern "C"`
- `src/encoder/main.cpp` has a `main()` function
</acceptance_criteria>

### Task 01-02-02: Create Player Skeleton

<read_first>
- CMakeLists.txt (root — to see FetchContent targets and include paths)
- include/ascv/format.hpp (shared header to include)
</read_first>

<action>
Create `src/player/CMakeLists.txt` with:
- `add_executable(ascv_player main.cpp)`
- `target_link_libraries(ascv_player PRIVATE miniaudio libzstd_static)`
- `target_include_directories(ascv_player PRIVATE ${PROJECT_SOURCE_DIR}/include)`

Create `src/player/main.cpp` with:
- `#include "ascv/format.hpp"`
- `#include <iostream>`
- `int main(int argc, char* argv[])` that prints `"ASCV Player v" << ascv::FORMAT_VERSION` and returns 0
</action>

<acceptance_criteria>
- `src/player/CMakeLists.txt` exists and contains `add_executable(ascv_player`
- `src/player/CMakeLists.txt` links `miniaudio` and `libzstd_static`
- `src/player/main.cpp` includes `ascv/format.hpp`
- `src/player/main.cpp` has a `main()` function
</acceptance_criteria>

---

## Wave 3: Build Verification

### Task 01-03-01: Full Build Verification

<read_first>
- CMakeLists.txt
- src/encoder/CMakeLists.txt
- src/player/CMakeLists.txt
</read_first>

<action>
Run the full build pipeline:
1. `cmake -B build` — configure the project
2. `cmake --build build` — build both targets
3. Verify both `ascv_encoder` and `ascv_player` binaries exist in the build directory
4. Run `./build/src/encoder/ascv_encoder` and verify it prints version info
5. Run `./build/src/player/ascv_player` and verify it prints version info
Fix any compilation errors until both targets build cleanly.
</action>

<acceptance_criteria>
- `cmake -B build` exits with code 0
- `cmake --build build` exits with code 0 with no errors
- Binary `build/src/encoder/ascv_encoder` exists and is executable
- Binary `build/src/player/ascv_player` exists and is executable
- Running `./build/src/encoder/ascv_encoder` prints output containing "ASCV Encoder"
- Running `./build/src/player/ascv_player` prints output containing "ASCV Player"
</acceptance_criteria>

<automated>
cmake -B build && cmake --build build && ./build/src/encoder/ascv_encoder && ./build/src/player/ascv_player
</automated>

---

## Artifacts this phase produces

### New files
- `CMakeLists.txt` — Root CMake configuration with C++20, FetchContent (zstd, miniaudio), PkgConfig (FFmpeg)
- `include/ascv/format.hpp` — Binary format specification header
- `src/encoder/CMakeLists.txt` — Encoder build configuration
- `src/encoder/main.cpp` — Encoder entry point skeleton
- `src/player/CMakeLists.txt` — Player build configuration
- `src/player/main.cpp` — Player entry point skeleton
- `.clang-format` — Code formatting configuration

### Symbols created
- `namespace ascv` — Top-level namespace for shared code
- `ascv::MAGIC[4]` — Magic bytes constant `{'A','S','C','V'}`
- `ascv::FORMAT_VERSION` — Format version constant
- `ascv::FileHeader` — Packed struct for `.ascv` file header
- `ascv::validate_magic()` — Magic bytes validation helper
- `ascv_encoder` — CMake executable target (Encoder)
- `ascv_player` — CMake executable target (Player)
