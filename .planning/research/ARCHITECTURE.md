# Architecture Research

**Domain:** Terminal-based Video Systems (ASCV format)
**Researched:** 2026-06-15
**Confidence:** HIGH

## Standard Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      Offline Processing                     │
├─────────────────────────────────────────────────────────────┤
│  ┌────────────┐     ┌─────────────┐     ┌────────────────┐  │
│  │ Media File │ ──> │   FFmpeg    │ ──> │ Pixel-to-ASCII │  │
│  │ (mp4, mkv) │     │ (Demux/Dec) │     │  Transformer   │  │
│  └────────────┘     └─────────────┘     └────────┬───────┘  │
│                                                  │          │
│  ┌────────────┐     ┌─────────────┐     ┌────────▼───────┐  │
│  │ ASCV File  │ <── │ Compressor  │ <── │  Frame Packer  │  │
│  │   (.ascv)  │     │ (Zstd+RLE)  │     │  (Chunking)    │  │
│  └─────┬──────┘     └─────────────┘     └────────────────┘  │
└────────┼────────────────────────────────────────────────────┘
         │
┌────────┼────────────────────────────────────────────────────┐
│        ▼               Realtime Playback                    │
├────────┼────────────────────────────────────────────────────┤
│  ┌─────┴──────┐     ┌─────────────┐     ┌────────────────┐  │
│  │    ASCV    │ ──> │ Decompress  │ ──> │   Sync Engine  │  │
│  │   Parser   │     │ (Zstd+RLE)  │     │ (Audio/Video)  │  │
│  └────────────┘     └─────────────┘     └────────┬───────┘  │
│                                                  │          │
│  ┌────────────┐     ┌─────────────┐     ┌────────▼───────┐  │
│  │  Terminal  │ <── │  Raw ANSI   │     │   MiniAudio    │  │
│  │   stdout   │     │  Renderer   │     │    Backend     │  │
│  └────────────┘     └─────────────┘     └────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|----------------|------------------------|
| **Encoder: Demuxer/Decoder** | Reading raw video/audio tracks. | `libavcodec` / `libavformat` (FFmpeg) |
| **Encoder: Transformer** | Mapping RGB pixels to characters and ANSI color sequences. | Custom C++ logic, sampling matrices |
| **Encoder: Packer** | Organizing video frames and audio chunks, generating the `.ascv` file structure. | Custom C/C++ struct serialization |
| **Encoder: Compressor** | Squeezing text/ANSI strings using Run-Length Encoding and Zstd. | Custom RLE pass + `libzstd` |
| **Player: Parser** | Reading `.ascv` header, index, and streaming chunks into memory. | Memory mapped file (`mmap`) or buffered I/O |
| **Player: Decompressor**| Expanding Zstd and RLE to raw ANSI sequences frame by frame. | `libzstd` + custom RLE decode |
| **Player: Sync Engine** | Maintaining the framerate against an audio clock. | Custom timing loop, checking MiniAudio time |
| **Player: Renderer** | Directly writing to the terminal with zero allocations per frame. | Raw `write()` to `stdout` with `termios`/`SetConsoleMode` |
| **Player: Audio** | Managing audio device output. | `miniaudio.h` |

## Recommended Project Structure

```
ASCV/
├── src/
│   ├── common/           # Shared types, file format spec, compression utilities
│   │   ├── ascv_format.h
│   │   ├── rle.h
│   │   └── rle.c
│   ├── encoder/          # The offline video processor (Heavy)
│   │   ├── main.cpp
│   │   ├── demuxer.cpp   # FFmpeg wrappers
│   │   ├── ascii_map.cpp # Video-to-text algorithms
│   │   └── writer.cpp    # Zstd compression and file writing
│   └── player/           # The terminal renderer (Lightweight)
│       ├── main.c
│       ├── terminal.c    # raw mode, ansi sequences (termios/winapi)
│       ├── audio.c       # MiniAudio wrapper
│       └── render.c      # Loop, zstd inflate, frame dump
├── include/              # Public headers if building a library
├── third_party/          # Dependencies (MiniAudio, zstd)
└── CMakeLists.txt        # Build definitions
```

### Structure Rationale

- **common/:** Both the Encoder and Player need absolute agreement on the `.ascv` file structure (magic bytes, headers, chunk formats) and the RLE logic.
- **encoder/:** Isolated because it relies on heavy dependencies (FFmpeg). Written typically in C++ to better interface with video processing libraries and manage complex memory models.
- **player/:** Kept highly lightweight, possibly pure C, maximizing bare-metal performance. Avoids any FFmpeg headers.
- **third_party/:** To ensure deterministic, easy builds for `miniaudio` and `zstd` across platforms without relying heavily on system packages (except FFmpeg for the encoder).

## Architectural Patterns

### Pattern 1: Pre-computed Rendering Pipeline (Zero-Load Playback)

**What:** Shift 100% of the display math (calculating characters, background colors, cursor moves) from the player into the encoder. The output format is just raw ANSI escape strings.
**When to use:** When target environments (terminal emulators) have highly constrained rendering performance, and host CPUs shouldn't be taxed to watch a video.
**Trade-offs:** Drastically increases file size compared to raw text, which mandates strong compression (Zstd+RLE). It tightly couples the encoded file to a specific resolution/color constraint chosen at encoding time.

**Example:**
```cpp
// Encoder does the heavy lifting
std::string ansi_frame = build_ansi_from_pixels(pixels);
write_to_file(zstd_compress(rle_compress(ansi_frame)));

// Player does literally this
write(STDOUT_FILENO, raw_decompressed_frame_data, frame_size);
```

### Pattern 2: Memory-Mapped Playback

**What:** Using `mmap` (or Windows equivalent) to map the `.ascv` file into memory rather than performing explicit `read()` calls in a loop.
**When to use:** For high-bandwidth data reading where copying into application buffers introduces latency.
**Trade-offs:** Can be tricky with extremely large files on 32-bit systems, but perfect for 64-bit systems. Simplifies parsing logic at the cost of less portability.

### Pattern 3: Audio-Driven Clock

**What:** Instead of relying on `sleep()` or system timers for video framerate, use the audio playback position as the master clock.
**When to use:** Any synchronized A/V playback system.
**Trade-offs:** Video might drop frames if it falls behind, but audio won't crackle or drift.

## Data Flow

### Encode Flow (Offline)

```
[mp4/mkv] 
    ↓ (FFmpeg decode)
[Raw RGB Frames & PCM Audio]
    ↓ (Transformer)
[ANSI Escaped Text Frames]
    ↓ (RLE -> Zstd)
[Compressed Chunks]
    ↓ (Muxer)
[.ascv File]
```

### Playback Flow (Realtime)

```
[.ascv File] 
    ↓ (mmap / read)
[Chunk Demuxer] ──────(Audio Data)─────> [MiniAudio Buffer]
    ↓ (Video Data)
[Zstd -> RLE Decompress]
    ↓
[Raw ANSI Frame Buffer]
    ↓ (Sync with Audio Clock)
[stdout (Terminal)]
```

## Build Order & Integration Plan

Because the Player needs an ASCV file to work, and the Encoder needs a format spec, the build order is crucial.

1.  **Phase 1: Foundation (`common/`)**
    *   Define the `.ascv` binary header, magic bytes, and chunk layout.
2.  **Phase 2: Minimal Encoder (`encoder/`)**
    *   Integrate FFmpeg. Extract uncompressed grayscale frames to simple text (no compression).
3.  **Phase 3: Minimal Player (`player/`)**
    *   Set terminal raw mode (`termios`/`SetConsoleMode`).
    *   Read the uncompressed `.ascv` file and blast frames to `stdout` in a basic loop.
4.  **Phase 4: Compression (`common/`)**
    *   Add RLE and Zstd. Update Encoder to write, Player to read.
5.  **Phase 5: Audio & Sync (`player/` & `encoder/`)**
    *   Extract PCM audio in Encoder.
    *   Implement MiniAudio playback in Player and tie frame rendering to the audio clock.
6.  **Phase 6: High Fidelity**
    *   Add 16/256/True color ANSI mapping in the Encoder.
    *   Implement seeking/scrubbing in the Player.

## Anti-Patterns

### Anti-Pattern 1: Ncurses/Termbox for Video Rendering

**What people do:** Use `ncurses` or `termbox` libraries to place characters on the screen.
**Why it's wrong:** These libraries maintain their own internal backbuffers and perform cell-by-cell diffing to minimize terminal writes. For full-screen video, this diffing is highly CPU intensive and slower than just dumping raw ANSI strings.
**Do this instead:** Build fully pre-baked raw ANSI string frames in the Encoder, and use raw `write()` to `STDOUT_FILENO` in the Player.

### Anti-Pattern 2: System-Timer Syncing

**What people do:** Use `nanosleep()` or `std::this_thread::sleep_for()` to manage framerate.
**Why it's wrong:** System thread scheduling is imprecise. Over a 5-minute video, the video will inevitably drift completely out of sync with the audio track.
**Do this instead:** Query the audio API (MiniAudio) for the current playback head position and display the video frame that matches that exact timestamp.

## Sources

- Terminal rendering performance techniques (Raw ANSI vs ncurses)
- FFmpeg libavcodec documentation
- MiniAudio single-header library architecture
- Zstd compression streaming API
---
*Architecture research for: ASCV (ASCII Video Format)*
*Researched: 2026-06-15*
