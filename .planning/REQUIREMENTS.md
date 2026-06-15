# Requirements

## ASCV Format & Render

### Core Rendering
- [ ] REQ-REND-01 (Phase 2): Support Basic ASCII (black and white, standard character set) output
- [ ] REQ-REND-02 (Phase 5): Support ANSI 16/256 colors output
- [ ] REQ-REND-03 (Phase 5): Support True color (24-bit RGB) ANSI output
- [ ] REQ-REND-04 (Phase 3): Use raw ANSI escape codes with `termios`/`SetConsoleMode` for terminal manipulation

### Audio & Sync
- [ ] REQ-AUDIO-01 (Phase 6): Implement synchronized audio playback using MiniAudio
- [ ] REQ-AUDIO-02 (Phase 7): Implement seeking/scrubbing support in the player

### Core Components
- [ ] REQ-COMP-01 (Phase 2): Implement an Encoder in C/C++ that decodes standard video (using FFmpeg libavcodec/libavformat) and encodes to `.ascv`
- [ ] REQ-COMP-02 (Phase 3): Implement a Player in C/C++ that reads `.ascv` and renders to the terminal
- [ ] REQ-COMP-03 (Phase 4): Use RLE + zstd layering for high-efficiency compression
- [ ] REQ-COMP-04 (Phase 1): Set up the project build system using CMake
- [ ] REQ-COMP-05 (Phase 4): Implement delta frames (X/Y coordinate changes) and periodic keyframes

## Out of Scope
- [ ] Real-time encoding/decoding of standard video formats during playback (Heavy processing must be offloaded to the one-time Encoding phase to ensure zero CPU load playback)
- [ ] Heavy dependencies like OpenCV for video decoding (Using FFmpeg for robust video decoding instead)

---
*Last updated: 2026-06-15 after initialization*
