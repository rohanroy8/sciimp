# Feature Research

**Domain:** Terminal-based ASCII Video Players and Formats (ASCV)
**Researched:** 2026-06-15
**Confidence:** HIGH

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist. Missing these = product feels incomplete.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Standard ASCII/B&W output | Baseline requirement for any "ASCII" terminal tool. | LOW | Standard character mapping (e.g. ` .:-=+*#%@`). |
| ANSI 16/256 Color | Terminals have had color for decades; users expect basic colorization. | MEDIUM | Standard ANSI escape codes for foreground/background. |
| Synchronized Audio | "Video" implies audio. Silent video is largely useless for modern media. | HIGH | Requires threading or precise timing loop with MiniAudio. |
| Play/Pause/Stop & Seeking | Basic media controls. If you can't scrub, it's just a long GIF. | MEDIUM | Requires index/chunking in the `.ascv` format to jump without linear decoding. |
| Graceful Resize Handling | Terminals get resized dynamically; visual output shouldn't break permanently. | MEDIUM | Signal handling (`SIGWINCH`) and clearing screen/re-centering. |

### Differentiators (Competitive Advantage)

Features that set the product apart. Not required, but valuable.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Zero CPU Load Playback | Differentiates from existing tools like `caca` or `timg` that decode mp4s in real-time, hogging CPU. | HIGH | Offloads math to a one-time Encoding step; Player just dumps pre-computed ANSI strings. |
| True Color (24-bit RGB) ANSI | Extremely high-fidelity "ASCII" that looks like pixel-art. | MEDIUM | Depends on modern terminal emulator support. |
| Ultra-Fast Decompression (RLE + zstd) | Huge files are a known issue with raw ANSI streams. High compression fixes this while keeping read speeds high. | HIGH | Requires careful binary format design for the `.ascv` file structure. |
| Subtitle / Closed Captioning | Rendering text over text is an interesting visual trick and highly accessible. | MEDIUM | Could be baked into the frame or rendered as a separate ANSI layer. |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create problems.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Real-time video decoding | Users want to "just play `video.mp4`". | Destroys the "Zero CPU" core value; requires huge dependencies (FFmpeg/OpenCV) in the player. | Provide an extremely fast 1-click Encoder CLI that converts `.mp4` to `.ascv` first. |
| Built-in GUI / Windowing | "Pop up a window if the terminal is too small." | Defeats the purpose of a terminal tool; drags in X11/Wayland/Win32 dependencies. | Stick strictly to raw ANSI and `termios`/`SetConsoleMode`. Gracefully degrade on small terms. |
| Complex UI (Menus, Playlists) | Makes it a "full app". | Bloats the lightweight player binary. | Stick to CLI arguments and basic keybinds (Space=Pause, Arrows=Seek). |

## Feature Dependencies

```
[Basic ASCII Output]
    └──requires──> [Raw ANSI Terminal Setup]

[True Color ANSI Output]
    └──requires──> [Basic ASCII Output]

[Zero CPU Playback]
    └──requires──> [Ultra-Fast Decompression]
                       └──requires──> [.ascv Binary Format]

[Seeking / Scrubbing]
    └──requires──> [Chunked / Indexed .ascv Format]

[Synchronized Audio] ──conflicts with──> [Variable Frame Rate Rendering]
```

### Dependency Notes

- **[True Color ANSI Output] requires [Basic ASCII Output]:** Core rendering loop must work before injecting 24-bit codes.
- **[Zero CPU Playback] requires [Ultra-Fast Decompression]:** To keep CPU low, reading the file and dumping to terminal must be bottlenecked by I/O, not decompression.
- **[Seeking / Scrubbing] requires [Chunked / Indexed .ascv Format]:** The player needs a way to byte-jump to specific timestamps without decoding the entire file linearly.
- **[Synchronized Audio] conflicts with [Variable Frame Rate Rendering]:** Audio requires strict timing; the player must lock the video render loop to the audio clock.

## MVP Definition

### Launch With (v1)

Minimum viable product — what's needed to validate the concept.

- [ ] One-Time Encoder (FFmpeg -> `.ascv`) — Needed to generate the test data.
- [ ] Raw ANSI Terminal Setup — Needed for high-speed terminal manipulation.
- [ ] Basic ASCII & 16-Color Output — Core rendering capability.
- [ ] Synchronized Audio (MiniAudio) — Essential for the "Video" experience.
- [ ] RLE + zstd Decompression — Validates the performance / size trade-off.

### Add After Validation (v1.x)

Features to add once core is working.

- [ ] True Color (24-bit RGB) Support — Enhances visual fidelity for modern terminals.
- [ ] Seeking and Scrubbing — Essential for longer videos, requires format indexing.
- [ ] Playback Keybinds (Pause/Stop) — Quality of life.

### Future Consideration (v2+)

Features to defer until product-market fit is established.

- [ ] Subtitles / Captions Rendering — Nice to have, adds rendering complexity.
- [ ] Streaming `.ascv` over Network — Potential pivot for terminal-based broadcasting (e.g., ASCII Twitch).

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Encoder -> `.ascv` Generator | HIGH | HIGH | P1 |
| Raw ANSI Rendering Engine | HIGH | MEDIUM | P1 |
| Basic/Color Output | HIGH | LOW | P1 |
| Synchronized Audio | HIGH | MEDIUM | P1 |
| Seeking/Scrubbing | MEDIUM | HIGH | P2 |
| True Color (24-bit) | HIGH | MEDIUM | P2 |
| Subtitles | LOW | MEDIUM | P3 |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

## Competitor Feature Analysis

| Feature | `caca` / `libcaca` | `timg` | Our Approach (ASCV) |
|---------|--------------------|--------|---------------------|
| Decoding overhead | High (real-time) | High (real-time) | Zero (Pre-encoded binary) |
| Color Support | Basic/ANSI | True Color / Sixel | ANSI Color up to 24-bit True Color |
| Audio Sync | Poor / None | None | Locked tight to MiniAudio clock |
| Target Use Case | Image viewing / fun | High-res image/video viewing | High-performance, synchronized media playback |

## Sources

- .planning/PROJECT.md
- Analysis of existing terminal tools (`timg`, `libcaca`, `mpv --vo=tct`)

---
*Feature research for: Terminal-based ASCII Video Players and Formats (ASCV)*
*Researched: 2026-06-15*
