# Project Research Summary

**Project:** ASCV
**Domain:** Terminal-based Video Systems (ASCII Video Format)
**Researched:** 2026-06-15
**Confidence:** HIGH

## Executive Summary

ASCV is a terminal-based media system designed to deliver high-performance, synchronized audio-video playback entirely within the command line. To achieve the core requirement of "virtually zero CPU load" during playback, the system must avoid real-time video decoding or color quantization entirely. 

The recommended approach bifurcates the system: an offline, ahead-of-time Encoder that heavily processes standard video files into a custom pre-computed `.ascv` binary format, and a lightweight, bare-metal Player that strictly decompresses and streams these pre-rendered ANSI frames to the terminal.

The most critical risks involve audio-video desynchronization over longer playtimes, terminal flickering from excessive unbuffered I/O, and state corruption from improper terminal lifecycle management. These are mitigated by using an audio-driven master clock, single-call buffered `write()` operations, and robust signal handling.

## Key Findings

### Recommended Stack

The stack relies on C++ for the heavy lifting in the Encoder, leveraging industry standards for multimedia and compression, while keeping the Player as close to bare metal as possible.

**Core technologies:**
- **C/C++ (C++20):** Core Language — Provides low-level memory control and maximum performance for binary manipulation and zero-allocation rendering loops.
- **FFmpeg (libavcodec/libavformat):** Video Decoding — Robust standard for reading multiple media formats; used exclusively in the offline Encoder.
- **Zstandard (zstd):** Compression — High compression ratios with exceptionally fast real-time decompression speeds, ideal for the Player.
- **MiniAudio:** Audio Playback — Lightweight, header-only C library for cross-platform audio that avoids heavy framework dependencies.

### Expected Features

**Must have (table stakes):**
- Standard ASCII and basic ANSI 16/256 color output.
- Synchronized audio playback.
- Graceful terminal resize handling and proper exit state restoration.
- A one-time Encoder CLI that converts standard media to `.ascv`.

**Should have (competitive):**
- Zero CPU load playback (the primary differentiator from existing tools).
- True Color (24-bit RGB) ANSI support for high fidelity.
- Ultra-fast decompression (RLE + zstd).
- Play/Pause/Stop and Seeking controls.

**Defer (v2+):**
- Subtitle/CC overlay rendering.
- Streaming `.ascv` over the network.

### Architecture Approach

The architecture enforces a strict divide between the offline Encoder and the realtime Player, joined by a shared understanding of the custom `.ascv` binary format. The Player uses a pre-computed rendering pipeline and an audio-driven clock to ensure performance.

**Major components:**
1. **Encoder (`encoder/`):** Heavy, offline processor that demuxes video/audio using FFmpeg, transforms pixels to ASCII/ANSI, compresses data (RLE+Zstd), and packs it into `.ascv`.
2. **Player (`player/`):** Lightweight, realtime loop that reads `.ascv` chunks, decompresses them, maintains sync against the MiniAudio clock, and dumps raw ANSI to `stdout`.
3. **Common (`common/`):** Shared types and binary definitions ensuring absolute agreement on the `.ascv` format spec and RLE logic.

### Critical Pitfalls

1. **Audio-Video Desynchronization** — Avoid naive `sleep()` loops; instead, use the audio stream playback position as the master clock to sync video frames.
2. **Excessive ANSI Escape Codes & Unbuffered I/O** — Avoid unbuffered character writing. Construct full frames in memory and use a single `write()` call, optimized with delta updates or RLE.
3. **Real-time Transcoding** — Avoid doing math or decoding in the player; offload all pixel-to-ASCII quantization to the offline Encoder to prevent CPU bottlenecks.
4. **Terminal State Corruption** — Avoid leaving the user in raw mode with a hidden cursor. Always trap `SIGINT` and resize events to clean up the terminal gracefully.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Foundation (Common Spec)
**Rationale:** The Encoder and Player need an absolute agreement on the `.ascv` file structure before any meaningful data can be passed.
**Delivers:** Binary header, magic bytes, and chunk layout definitions (`common/`).
**Addresses:** Ultra-Fast Decompression (structure prerequisite).
**Avoids:** Security mistakes regarding malformed files by defining a strict schema.

### Phase 2: Minimal Encoder
**Rationale:** We need test data to build the player. Establishing the FFmpeg pipeline early ensures we can decode input media.
**Delivers:** FFmpeg integration, extracting uncompressed grayscale frames to simple text.
**Uses:** C++, CMake, FFmpeg (libavcodec/libavformat).
**Implements:** Encoder component (Demuxer & Transformer).

### Phase 3: Minimal Player
**Rationale:** With minimal test data available, we can establish the core high-performance rendering loop.
**Delivers:** Terminal raw mode configuration (`termios`/`SetConsoleMode`) and basic `stdout` frame dumping.
**Uses:** C, OS-specific terminal APIs.
**Implements:** Player component (Renderer).
**Avoids:** Excessive I/O tearing by establishing the single-buffer `write()` pattern early.

### Phase 4: Compression pipeline
**Rationale:** Uncompressed ASCII video sizes are massive; compression is required before adding color or longer videos.
**Delivers:** RLE and Zstd integration into the Encoder (writer) and Player (decompressor).
**Uses:** Zstandard (zstd).
**Implements:** Compressor/Decompressor components.

### Phase 5: Audio & Synchronization
**Rationale:** Video playback without audio is incomplete; introducing it requires establishing the master timing loop.
**Delivers:** PCM audio extraction in Encoder, MiniAudio integration in Player, and audio-clock sync logic.
**Uses:** MiniAudio.
**Implements:** Player component (Sync Engine).
**Avoids:** Audio-video desync drift (Pitfall 1).

### Phase 6: High Fidelity & UX
**Rationale:** Polish features added after the core zero-CPU, synced loop is verified.
**Delivers:** 16/256/True color ANSI mapping, Seeking/Scrubbing controls, and graceful resize/teardown handling.
**Addresses:** True Color support, Playback Keybinds.
**Avoids:** Terminal state corruption on resize/exit.

### Phase Ordering Rationale

- The common format specification must exist first as it binds the two decoupled systems.
- The Encoder must precede the Player because the Player strictly relies on pre-computed `.ascv` data to function.
- Compression is prioritized before Audio and Color to solve fundamental I/O and file size bottlenecks early.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 4 (Compression pipeline):** RLE binary structure optimization requires careful byte-packing design to maximize Zstd efficiency.
- **Phase 5 (Audio & Synchronization):** Proper configuration of MiniAudio's low-latency block-mode and frame-accurate timing math will need specific focus.

Phases with standard patterns (skip research-phase):
- **Phase 3 (Minimal Player):** Standard UNIX/Windows terminal raw mode (`termios`/`SetConsoleMode`) is a well-established pattern.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Validated robust tools (FFmpeg, zstd, MiniAudio) with clear cross-platform support. |
| Features | HIGH | Clear distinction between Player requirements and Encoder responsibilities. |
| Architecture | HIGH | Complete decoupling of processing (Encoder) from rendering (Player) guarantees CPU constraints. |
| Pitfalls | HIGH | Known edge cases in terminal rendering and audio syncing are well documented. |

**Overall confidence:** HIGH

### Gaps to Address

- Cross-platform terminal color fidelity: Validating whether Windows requires specific `ENABLE_VIRTUAL_TERMINAL_PROCESSING` flags in all environments (Powershell, CMD, Windows Terminal) needs verification during implementation.
- Memory map limits: Ensuring `mmap` parsing scales gracefully with massive `.ascv` files on memory-constrained systems or 32-bit architectures.

## Sources

### Primary (HIGH confidence)
- `STACK.md` — Core technologies and dependencies.
- `FEATURES.md` — Requirement prioritization and differentiation.
- `ARCHITECTURE.md` — System overview and data flows.
- `PITFALLS.md` — Technical risks and mitigations.

### Secondary (MEDIUM confidence)
- FFmpeg and MiniAudio official documentation.
- Standard UNIX terminal rendering optimization guides.

---
*Research completed: 2026-06-15*
*Ready for roadmap: yes*
