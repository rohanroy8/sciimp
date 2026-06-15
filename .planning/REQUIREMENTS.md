# Requirements

## ASCV Format & Render

### Core Rendering
- [ ] REQ-REND-01: Support Basic ASCII (black and white, standard character set) output
- [ ] REQ-REND-02: Support ANSI 16/256 colors output
- [ ] REQ-REND-03: Support True color (24-bit RGB) ANSI output
- [ ] REQ-REND-04: Use raw ANSI escape codes with `termios`/`SetConsoleMode` for terminal manipulation

### Audio & Sync
- [ ] REQ-AUDIO-01: Implement synchronized audio playback using MiniAudio
- [ ] REQ-AUDIO-02: Implement seeking/scrubbing support in the player

### Core Components
- [ ] REQ-COMP-01: Implement an Encoder in C/C++ that decodes standard video (using FFmpeg libavcodec/libavformat) and encodes to `.ascv`
- [ ] REQ-COMP-02: Implement a Player in C/C++ that reads `.ascv` and renders to the terminal
- [ ] REQ-COMP-03: Use RLE + zstd layering for high-efficiency compression
- [ ] REQ-COMP-04: Set up the project build system using CMake

## Out of Scope
- [ ] Real-time encoding/decoding of standard video formats during playback (Heavy processing must be offloaded to the one-time Encoding phase to ensure zero CPU load playback)
- [ ] Heavy dependencies like OpenCV for video decoding (Using FFmpeg for robust video decoding instead)

---
*Last updated: 2026-06-15 after initialization*
