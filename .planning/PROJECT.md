# ASCV (ASCII Video Format)

## What This Is

A proprietary, high-performance binary video format (`.ascv`) and rendering engine designed specifically for terminal-based ASCII playback. It offloads heavy processing like video decoding and pixel-to-text math to a one-time "Encoding" phase, producing a highly compressed binary file. A specialized terminal player can then render this file with virtually zero CPU load.

## Core Value

Achieve high-fidelity, synchronized ASCII video and audio playback in the terminal with virtually zero CPU load during playback.

## Current State

**v1.0 Shipped (2026-06-19)**
Delivered the complete offline encoder, terminal player, A/V synchronization, and advanced binary compression (`.ascv`), satisfying the core performance constraints for zero-CPU playback.

## Next Milestone Goals

(To be defined in the next milestone planning session.)

## Context

- Target environments: Cross-platform terminal emulators (Linux, macOS, Windows).
- Performance is critical for the Player, hence the choice of C/C++, raw ANSI codes, and pre-computed frames.
- The project involves two main components: an Encoder (heavy lifting) and a Player (lightweight).

## Constraints

- **Language**: C/C++ — For maximum performance and control.
- **Build System**: CMake — For cross-platform support.
- **Audio API**: MiniAudio — Lightweight and header-only.
- **Terminal API**: Raw ANSI with `termios`/`SetConsoleMode` — For maximum rendering performance over ncurses/termbox.
- **Video Library**: FFmpeg — Needed for robust standard video decoding in the Encoder.

## Key Decisions

<!-- Decisions that constrain future work. Add throughout project lifecycle. -->

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Use C/C++ for both | Maximum performance and zero CPU load playback | — Pending |
| FFmpeg for decoding | Robust standard video support without real-time overhead | — Pending |
| RLE + zstd compression | High compression ratio with extremely fast decompression | — Pending |
| MiniAudio for audio | Header-only, cross-platform, easy to integrate | — Pending |
| Raw ANSI for terminal | Maximum performance and control over rendering | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-06-16 after completing Phase 02*
