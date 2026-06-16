# ASCV Testing Guide

This document outlines the testing strategy, tools, and procedures for the ASCV (ASCII Video Format) project. 

## Testing Strategy

Given the core goals of ASCV (a high-performance binary video format and a virtually zero-CPU-load terminal player), testing is divided into the following categories:

1. **Unit Testing (C++)**: Validates isolated logic, such as mathematical calculations (e.g., aspect ratio, padding), parsing, and data manipulation.
2. **Integration Testing (Bash / FFmpeg)**: Tests the end-to-end flow of the `ascv_encoder` by generating synthetic video files, running them through the encoder, and validating the resulting `.ascv` binary output byte-by-byte.

## Prerequisites

To run the test suite, ensure the following are installed:
- **CMake** (3.25+) and a C++ compiler (C++20)
- **FFmpeg**: Required both for linking the encoder and for the `ffmpeg` CLI tool, which is used to generate synthetic video data (`testsrc`) for integration testing.

## Running the Tests

Currently, the primary entry point for tests is the `test_encoder.sh` script, which also automatically runs the C++ unit tests.

### 1. Build the Project

Ensure the project and the test executables are built. Run the following from the root directory:

```bash
mkdir -p build
cmake -B build -S .
cmake --build build
```

*Note: The test scripts expect the build artifacts to reside in the `build/` directory.*

### 2. Run the Test Script

Execute the integration test script from the project root:

```bash
./tests/test_encoder.sh
```

This script will:
1. Generate a synthetic test video (`/tmp/ascv_test_input.mp4`) using FFmpeg.
2. Run `ascv_encoder` to encode the video to `.ascv`.
3. Validate the `.ascv` binary header (Magic bytes: `ASCV`, dimensions, framerate, frame count).
4. Verify the integrity of the frame data (printable ASCII check).
5. Run the compiled C++ unit tests (e.g., `test_aspect_ratio`).
6. Test edge cases such as padding (e.g., letterboxing/pillarboxing) using portrait/square video sources.
7. Clean up temporary files on completion.

## Test Frameworks and Tools

- **Catch2 (v3)**: Used for C++ unit tests (e.g., `test_aspect_ratio.cpp`). It is automatically fetched via CMake `FetchContent`.
- **Bash & Hexdump**: Used in `test_encoder.sh` to parse and validate the raw binary structure of the encoded `.ascv` files without relying on the player.

## Writing Tests

### Adding C++ Unit Tests
1. Create or update a test file in the `tests/` directory (e.g., `tests/test_myfeature.cpp`).
2. Add Catch2 `TEST_CASE` definitions.
3. Update `CMakeLists.txt` to include your new test executable or add your `.cpp` file to an existing test executable.
4. Ensure the new executable is invoked in the integration test script or add a CTest integration.

### Adding Integration Tests
1. Modify `tests/test_encoder.sh`.
2. Use `ffmpeg -f lavfi -i testsrc=...` to create reproducible, deterministic input videos.
3. Add steps to run the encoder and `hexdump` / `od` / `dd` commands to validate that the output matches expected parameters.

## Future Testing Scope

As the project matures, testing should be expanded to include:
- **Player Rendering Tests**: Outputting terminal ANSI codes to a file and validating the sequence.
- **Compression Tests**: Ensuring Zstandard (zstd) correctly compresses and decompresses payload data.
- **Audio Verification**: Validating that MiniAudio syncs appropriately and the `.ascv` file successfully stores interleaved audio tracks.
- **Memory Safety**: Running Valgrind/ASan routinely in the test suite to ensure frame-by-frame memory processing does not leak.
