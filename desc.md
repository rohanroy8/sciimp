# ASCV (ASCII Video) Codec & Player Concept

## 1. Project Overview
A proprietary, high-performance binary video format (`.ascv`) and rendering engine designed specifically for terminal-based ASCII playback. It offloads the heavy processing (video decoding, pixel-to-text math) to a one-time "Encoding" phase. The output is a highly compressed binary file that a specialized terminal player can render with virtually zero CPU load.

## 2. Phase 1: The C++ Encoder (Generator)
- **Decoding**: Uses FFmpeg's core C libraries (`libavcodec`, `libavformat`) to extract raw pixel memory from standard video formats (e.g., `.mp4`), bypassing high-level wrappers like OpenCV.
- **Processing**: Uses C++ multithreading and pre-calculated Lookup Tables (LUTs) to rapidly map pixel brightness and color to ASCII characters and ANSI color codes.
- **Compression**:
  - **Run-Length Encoding (RLE)**: Compresses repeating characters (e.g., storing 50 black spaces as a single instruction instead of 50 individual characters).
  - **Delta Frames (Inter-frame Compression)**: Saves only the characters (and their X/Y coordinates) that have changed from the previous frame. Keyframes are used periodically to refresh the entire grid.

## 3. Phase 2: The Lightweight Terminal Player
- **Execution**: A bare-metal executable (built in C or C++) requiring almost zero CPU overhead. It reads pre-calculated binary data into RAM, sets an internal timer (e.g., 33.3ms for 30 FPS), and pushes text blocks directly to standard output (`stdout`).
- **Target Audience**: Extremely low-power hardware, microcontrollers, legacy systems, or simply anyone running terminal-based applications without the CPU load of real-time conversion.

## 4. The Binary File Structure (.ascv)
The `.ascv` file is a strict binary layout designed to be read directly into memory blocks (structs) for maximum efficiency.

### Zone 1: The Global Header (16 Bytes)
Sets the rules for memory allocation and playback before the video starts:
- **Bytes 0-3 (Magic Number)**: `ASCV` (`0x41 0x53 0x43 0x56`) to verify the file type.
- **Byte 4 (Version)**: e.g., `0x01` (for future codec updates).
- **Byte 5 (Color Mode)**: `0x00` (B&W), `0x01` (16-color), `0x02` (True Color).
- **Bytes 6-7 (Width)**: Unsigned 16-bit integer (e.g., 120 columns).
- **Bytes 8-9 (Height)**: Unsigned 16-bit integer (e.g., 40 rows).
- **Byte 10 (FPS)**: Unsigned 8-bit integer (e.g., 30).
- **Bytes 11-14 (Total Frames)**: Unsigned 32-bit integer.
- **Byte 15 (Reserved)**: Empty (`0x00`) for future use or byte-alignment.

### Zone 2: The Data Stream (Frame Blocks)
Continuous stream of video frames following the header.
- **Frame Header (4 Bytes)**:
  - **Byte 0 (Frame Type)**: `K` for Keyframe (full screen refresh), `D` for Delta frame (only changes).
  - **Bytes 1-3 (Payload Size)**: 24-bit integer specifying the exact size of the incoming compressed payload in bytes.
- **The Payload (Compressed Data)**:
  - **Keyframe (K)**: Pairs a Count with a Character/Color. Binary layout: `[Count (1 byte)] [Char (1 byte)] [Color Code (1-3 bytes)]`. Overwrites the entire master grid in memory.
  - **Delta Frame (D)**: Contains coordinates and instructions. Binary layout: `[X Coord (1 byte)] [Y Coord (1 byte)] [Count (1 byte)] [Char (1 byte)] [Color Code]`. Jumps to specific coordinates and overwrites only those characters.

## 5. Audio Synchronization (New Considerations)
Audio synchronization shifts the engine from a "fixed FPS loop" to a "clock-driven model". Audio glitches are highly noticeable, so audio timing serves as the master clock.

- **Presentation Timestamps (PTS)**: Moving away from pure fixed FPS. The frame format should store PTS per frame, giving the player the ability to drop or duplicate frames to stay synced with audio and avoid drifting.
- **Audio Embedding vs Sidecar**: Audio needs its own pipeline. It can be embedded as raw/compressed PCM directly into `.ascv` (requiring a larger header and complex seeking) or provided as a separate sidecar file (easier MVP).
- **Playback Backend**: The player needs a way to play audio—either by shelling out to an external tool like `ffplay`, or directly linking lightweight libraries like `PortAudio` or `miniaudio`.
- **Header Changes**: If audio is embedded, the header must be expanded with an audio offset, format data (sample rate, channels, bit depth), and the size of the payload.

## 6. Prior Art & The Novelty of ASCV
- **What Exists**: Live real-time ASCII conversion tools (`tmedia`, `ascii-video-player`) exist, but they heavily tax the CPU. `asciinema` records text data as a custom binary, but it's meant for typing sessions, not 30 FPS multi-color video frames.
- **Why ASCV is Unique**: Pre-encoded offline compression (`.ascv` binary with RLE/Delta logic) combined with a bare-metal streaming player is **genuinely novel**. It solves the performance bottleneck existing ASCII players face.

## 7. Real Effort Sinks & Things to Ponder
Before starting, keep these critical constraints in mind:
- **Color Mapping is Difficult**: Mapping standard RGB values to a 16 or 256 ANSI color palette looks muddy if done with naive math. You need careful testing against real terminal palettes to get clean visual fidelity.
- **Delta Frames with Color**: A "changed cell" can mean a character changed, its color changed, or both. The binary format needs clean flagging, otherwise it will result in parsing ambiguity or wasted bytes.
- **ANSI Escape Sequence Overhead**: Even with low CPU decode, constantly printing ANSI escape codes for every color change takes processing power. Benchmark this early.
- **Header Future-Proofing**: A 16-byte header gets cramped quickly. Consider expanding it early to avoid painful refactoring later when adding audio metadata or codec flags.
- **The Master Clock**: Decide early to use audio as the master clock. It makes the rendering loop more complex than `sleep 33ms`, requiring drift correction.
- **Target Aspect Ratios & Resizing**: How do you handle terminal windows that don't match your 120x40 standard? Should the video letterbox, or require a specific terminal layout?
- **Platform Quirks**: Linux `tty` and Windows `ConPTY` handle ANSI coloring very differently. Define the minimum supported terminal environment up front.
- **MVP Scope Control**: Start extremely small. Build Black & White + RLE + fixed FPS + no audio first. Get that pipeline working end-to-end, and only then layer in delta frames, color codes, and audio sync.
