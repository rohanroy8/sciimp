# Project Roadmap

## Phases

### Phase 1: Foundation & Build System

**Goal:** Set up the core project structure, CMake build system, and define the common `.ascv` binary format specification.
**Mode:** mvp

**Requirements:**

- REQ-COMP-04

**Success Criteria:**

- Running `cmake` successfully configures the project for Encoder and Player executables.
- Common headers compiling without errors exist in a shared directory.

### Phase 2: Minimal Encoder

**Goal:** Implement the offline Encoder to extract frames from video files using FFmpeg and write them as raw basic ASCII to a minimal `.ascv` file.
**Mode:** mvp

**Requirements:**

- REQ-COMP-01
- REQ-REND-01

**Success Criteria:**

- Running the Encoder CLI against a test video produces a valid `.ascv` output file.
- The generated `.ascv` file contains the correct magic bytes and verifiable raw ASCII frame data.

### Phase 3: Minimal Player

**Goal:** Implement the realtime Player using raw terminal manipulation to read minimal `.ascv` files and render basic ASCII to stdout.
**Mode:** mvp

**Requirements:**

- REQ-COMP-02
- REQ-REND-04

**Success Criteria:**

- Running the Player CLI against the `.ascv` file renders the ASCII video frames to the terminal.
- Terminal is properly reset to its normal state when the player exits or receives SIGINT.

### Phase 4: Compression Pipeline

**Goal:** Integrate RLE, zstd compression, and delta frames with periodic keyframes into both the Encoder and Player to reduce `.ascv` file sizes and maintain fast rendering.
**Mode:** mvp

**Requirements:**

- REQ-COMP-03
- REQ-COMP-05

**Success Criteria:**

- The Encoder produces heavily compressed `.ascv` files utilizing RLE, zstd, and delta frames.
- The Player successfully decompresses and renders the compressed `.ascv` files in real-time, and decoded delta frames match the source keyframe + applied deltas byte-for-byte against a reference.

### Phase 5: High Fidelity Colors

**Goal:** Upgrade the rendering pipeline to support 16/256 ANSI colors and 24-bit True Color RGB output.
**Mode:** mvp

**Requirements:**

- REQ-REND-02
- REQ-REND-03

**Success Criteria:**

- The Encoder extracts color information and the Player outputs the video using ANSI 16/256 colors.
- High-fidelity terminal emulators successfully display the video in True Color (24-bit RGB) and output uses single buffered writes per frame to avoid partial-frame rendering.

### Phase 6: Synchronized Audio Playback

**Goal:** Introduce synchronized audio playback using MiniAudio for basic playback sync without scrubbing.
**Mode:** mvp

**Requirements:**

- REQ-AUDIO-01

**Success Criteria:**

- The Player outputs synchronized audio alongside the video playback.
- Basic playback sync is maintained continuously during normal forward playback.

### Phase 7: Seeking/Scrubbing

**Goal:** Implement playback controls (seeking/scrubbing) in the Player, including jumping to points, finding keyframes, and re-establishing audio position.
**Mode:** mvp

**Requirements:**

- REQ-AUDIO-02

**Success Criteria:**

- The user can scrub forward and backward in the video.
- The player successfully finds the nearest keyframe, applies deltas, and re-establishes the correct audio position to keep audio and video in sync after seeking.

### Phase 8: Compression Optimizations

**Goal:** Implement Keyframe Prediction Chain (P-frames based on previous frame) and Frame-Level Change Detection to drastically reduce `.ascv` file size.
**Mode:** mvp

**Requirements:**

- REQ-COMP-05

**Success Criteria:**

- The Encoder calculates deltas against the immediately preceding frame (`P(prev)`) rather than the `I-frame`.
- The Encoder correctly skips identical consecutive frames using a `REPEAT_FRAME` or skip-counter logic.
- The Player successfully decodes `P(prev)` streams and correctly processes repeated/skipped frames.
- Output file size for static or panning scenes drops significantly while visual fidelity remains unchanged.

### Phase 9: Character and Color Stream Separation

**Goal:** Separate the character and color streams before compression to allow RLE and Zstd to find much longer continuous matches.
**Mode:** mvp

**Requirements:**

- REQ-COMP-05

**Success Criteria:**

- The Encoder serializes the Glyph Plane and Color Plane independently before RLE and Zstd compression.
- The Player successfully decodes the separated streams and reassembles the frame buffer.
- File sizes for standard RGB videos drop significantly compared to interleaved compression.

### Phase 10: Zstd Dictionaries

**Goal:** Train and use a Zstd dictionary on ASCV frame payloads to learn repeated ANSI sequences, frame headers, and delta patterns.
**Mode:** mvp

**Requirements:**

- REQ-COMP-05

**Success Criteria:**

- The Encoder includes a training step or accepts a pre-trained Zstd dictionary for compression.
- The Player initializes its Zstd context with the dictionary before decompression.
- File sizes improve by 10-30% without changing core encoding logic.

### Phase 11: Palette Compression

**Goal:** Implement Palette Compression for RGB24 mode to reduce 3-byte colors to a 1-byte palette index.
**Mode:** mvp

**Requirements:**

- REQ-COMP-05

**Success Criteria:**

- The Encoder extracts an optimized color palette per frame or per video and stores it in the header.
- The Encoder writes a 1-byte palette index per cell instead of 3 bytes for RGB24.
- The Player successfully reads the palette and maps indices back to RGB colors during rendering.

### Phase 12: Motion Compensation

**Goal:** Implement `MOVE_BLOCK` commands in P-frames to optimize camera panning and motion.
**Mode:** mvp

**Requirements:**

- REQ-COMP-05

**Success Criteria:**

- The Encoder detects simple block translations (e.g., camera panning) and encodes them as `MOVE_BLOCK` operations rather than delta-updating every cell.
- The Player correctly executes `MOVE_BLOCK` instructions by copying memory regions within the frame buffer before applying further deltas.
- Significant file size reduction on scenes with camera movement.
