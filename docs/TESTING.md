# ASCV Testing Guide

This document outlines the testing strategy, tools, and procedures for the ASCV (ASCII Video Format) project.

## Testing Strategy

Given the core goals of ASCV (a high-performance binary video format and a virtually zero-CPU-load terminal player), testing is divided into the following categories:

1. **Unit Testing (C++)**: Validates isolated logic, such as mathematical calculations (e.g., aspect ratio, padding), parsing, and data manipulation.
2. **Integration Testing (Bash / FFmpeg)**: Tests the end-to-end flow of encoding and playing back video, validating frames, testing audio synchronization, and verifying terminal state resets.

## Prerequisites

To run the test suite, ensure the following are installed:
- **CMake** (3.25+) and a C++ compiler (C++20)
- **FFmpeg**: Required both for linking the encoder and for the `ffmpeg` CLI tool, which is used to generate synthetic video and audio data for integration testing.

## Running the Tests

Integration tests are separated into three main scripts in the `tests/` directory:

### 1. Build the Project

Ensure the project and the test executables are built. Run the following from the root directory:

```bash
mkdir -p build
cd build
cmake -G Ninja ..
ninja
```

*Note: The test scripts expect the build artifacts to reside in the `build/` directory.*

### 2. Run the Test Scripts

Execute the integration test scripts from the project root:

#### Encoder Integration Tests
```bash
./tests/test_encoder.sh
```
This script will:
- Generate a synthetic test video (`/tmp/ascv_test_input.mp4`) using FFmpeg.
- Run `ascv_encoder` to encode the video.
- Validate the `.ascv` binary header (magic bytes: `ASCV`, resolution, framerate, frame count).
- Verify the integrity of the frame data (printable ASCII check).
- Run the compiled C++ unit tests (`test_aspect_ratio`).
- Test padding edge cases (letterboxing/pillarboxing) using portrait/square video sources.

#### Player Integration Tests
```bash
./tests/test_player.sh
```
This script will:
- Generate a test video and encode it to `.ascv`.
- Play it back using `ascv_player`, outputting ANSI codes to a file.
- Validate that the output starts with the proper escape codes (e.g., `\x1b[H` for cursor home) and that terminal setup and restores operate correctly without blocking.

#### Audio and Pacing Integration Tests
```bash
./tests/test_audio.sh
```
This script will:
- Generate a test video containing a synthetic audio stream (sine wave).
- Run the encoder to generate the `.ascv` video and a corresponding sidecar `.wav` track.
- Verify WAV file headers.
- Run `ascv_player` to play the video with the WAV sidecar, validating sound synchronization, pacing, and clean shutdown on completion.

## Test Frameworks and Tools

- **Catch2 (v3)**: Used for C++ unit tests (e.g., `tests/test_aspect_ratio.cpp`). It is automatically fetched via CMake `FetchContent`.
- **Bash, Hexdump & od**: Used in integration scripts to parse and validate the raw binary structure of encoded `.ascv` files and terminal ANSI stream outputs.

## Writing Tests

### Adding C++ Unit Tests
1. Create or update a test file in the `tests/` directory (e.g., `tests/test_myfeature.cpp`).
2. Add Catch2 `TEST_CASE` definitions.
3. Update `CMakeLists.txt` to include your new test executable or add your `.cpp` file to an existing test executable.
4. Ensure the new executable is invoked in the integration test script or add a CTest integration.

### Adding Integration Tests
1. Modify or create scripts under `tests/`.
2. Use `ffmpeg -f lavfi -i ...` to generate reproducible, deterministic input videos.
3. Add steps to run the encoder/player and use standard Linux tools (`od`, `dd`, `wc`) to validate that stdout and file structures match expected dimensions and timings.

## Future Testing Scope

As the project matures, testing should be expanded to include:
- **CI/CD Integration**: Automatically running `test_encoder.sh`, `test_player.sh`, and `test_audio.sh` on every pull request using GitHub Actions.
- **Memory Safety (Routine Sanitizers)**: Running Valgrind or AddressSanitizer (ASan) routinely in the CI test suite to ensure frame-by-frame memory processing and zero-allocation paths do not leak memory or overflow bounds.
