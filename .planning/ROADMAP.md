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
