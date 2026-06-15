<!-- GSD:project-start source:PROJECT.md -->

## Project

**ASCV (ASCII Video Format)**

A proprietary, high-performance binary video format (`.ascv`) and rendering engine designed specifically for terminal-based ASCII playback. It offloads heavy processing like video decoding and pixel-to-text math to a one-time "Encoding" phase, producing a highly compressed binary file. A specialized terminal player can then render this file with virtually zero CPU load.

**Core Value:** Achieve high-fidelity, synchronized ASCII video and audio playback in the terminal with virtually zero CPU load during playback.

### Constraints

- **Language**: C/C++ — For maximum performance and control.
- **Build System**: CMake — For cross-platform support.
- **Audio API**: MiniAudio — Lightweight and header-only.
- **Terminal API**: Raw ANSI with `termios`/`SetConsoleMode` — For maximum rendering performance over ncurses/termbox.
- **Video Library**: FFmpeg — Needed for robust standard video decoding in the Encoder.

<!-- GSD:project-end -->

<!-- GSD:stack-start source:research/STACK.md -->

## Technology Stack

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| C++ | C++20 | Core Language | Provides maximum performance and low-level system access required for zero-CPU-load rendering. C++20 provides modern features like `std::span` and concepts that make binary manipulation safer. |
| CMake | 3.25+ | Build System | The defacto standard for C/C++ cross-platform builds. Essential for managing complex dependencies like FFmpeg across Linux, macOS, and Windows. |
| FFmpeg (libavcodec/libavformat) | 8.1+ | Video Decoding | The industry standard for robust video decoding. Handles almost any input format without bringing in heavy GUI or ML dependencies like OpenCV. Offloads all heavy lifting to the Encoder phase. |
| Zstandard (zstd) | 1.5.7+ | Compression | Offers an incredible balance of compression ratio and extremely fast decompression speed. Ideal for terminal video where fast real-time decompression is required during playback. |
| MiniAudio | 0.11+ | Audio Playback | A lightweight, header-only C library for audio playback. Eliminates the need to link heavy audio frameworks and works seamlessly across platforms. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| CLI11 | 2.4+ | Command Line Parsing | Use for handling complex CLI arguments for the Encoder (e.g., input file, output file, resolution, color mode). |
| spdlog | 1.14+ | Logging | Use for high-performance logging during the encoding phase to track progress and debug decoding issues without slowing down the pipeline. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| Ninja | Build backend | Use with CMake for significantly faster build times compared to Make. |
| Clang-Format | Code Formatting | Ensure consistent C++ style across the project. Use a standard `.clang-format` file (e.g., Google or LLVM style). |
| Valgrind / ASan | Memory Debugging | Crucial for ensuring no memory leaks occur during frame-by-frame video processing and raw pointer manipulation. |

## Installation

# Ubuntu/Debian Core Dependencies

# macOS Core Dependencies

# Windows (vcpkg)

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| FFmpeg | libvpx / libx264 directly | When you want to strictly limit the input format (e.g., only WebM/VP9) and drastically reduce the Encoder's dependency footprint. |
| Raw ANSI + `termios` | termbox2 / ncurses | When you want an easier cross-platform terminal abstraction and don't care about the absolute maximum rendering performance or precise raw ANSI sequence control. |
| Zstandard (zstd) | LZ4 | If zstd decompression is somehow a bottleneck (highly unlikely) and you are willing to sacrifice compression ratio for marginal decompression speed gains. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| OpenCV | Extremely heavy, pulls in GUI/ML dependencies, and is overkill just for reading video frames. | FFmpeg (`libavcodec` / `libavformat`) |
| Unbuffered `printf` / `std::cout` | Writing character-by-character or line-by-line via standard IO functions introduces massive overhead and tearing. | Allocate a large frame buffer and write the entire frame using a single `write()` system call to `STDOUT_FILENO`. |
| Scripting Languages (Python/Node.js) for Player | Cannot guarantee the "virtually zero CPU load" requirement due to garbage collection and runtime overhead. | C or C++ |
| Real-time Video Decoding in Player | Violates the core architecture goal of offloading heavy processing to the one-time Encoder phase. | Pre-compute ASCII frames and pack them into the `.ascv` binary format. |

## Stack Patterns by Variant

- Use `SetConsoleMode` with `ENABLE_VIRTUAL_TERMINAL_PROCESSING`.
- Because Windows Command Prompt / PowerShell needs explicit activation to process ANSI escape codes efficiently.
- Use Run-Length Encoding (RLE) heavily tailored to background/foreground color runs.
- Because raw ASCII text combined with 24-bit ANSI color codes can balloon frame sizes, and large blocks of the same color are common.

## Version Compatibility

| Package A | Compatible With | Notes |
|-----------|-----------------|-------|
| FFmpeg 8.x | C++20 | FFmpeg is a C library; ensure you wrap headers in `extern "C" {}` when including them in C++ files. |

## Sources

- Project Context (`PROJECT.md`) — Architectural constraints, goals, and out-of-scope boundaries.
- FFmpeg Official Docs / Releases — Verified FFmpeg 8.1+ as the latest stable version line for robust multimedia handling.
- Zstandard Official GitHub — Verified 1.5.7+ as the current leading fast-decompression algorithm.
- General C++ Terminal Rendering Best Practices — High confidence in single-buffer `write()` approach over standard IO for terminal video rendering.

<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->

## Conventions

Conventions not yet established. Will populate as patterns emerge during development.
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->

## Architecture

Architecture not yet mapped. Follow existing patterns found in the codebase.
<!-- GSD:architecture-end -->

<!-- GSD:skills-start source:skills/ -->

## Project Skills

No project skills found. Add skills to any of: `.agent/skills/`, `.agents/skills/`, `.cursor/skills/`, `.github/skills/`, or `.codex/skills/` with a `SKILL.md` index file.
<!-- GSD:skills-end -->

<!-- GSD:workflow-start source:GSD defaults -->

## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:

- `/gsd-quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd-debug` for investigation and bug fixing
- `/gsd-execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->

<!-- GSD:profile-start -->

## Developer Profile

> Profile not yet configured. Run `/gsd-profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
