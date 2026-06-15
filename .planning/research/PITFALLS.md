# Pitfalls Research

**Domain:** Terminal-based High-Performance Video Projects
**Researched:** 2026-06-15
**Confidence:** HIGH

## Critical Pitfalls

### Pitfall 1: Audio-Video Desynchronization

**What goes wrong:**
Audio and video start in sync but drift apart over time. Lip sync is lost, and sound effects occur seconds after visual events.

**Why it happens:**
Developers often rely on a naive sleep loop (`sleep(1/FPS)`) for the main video loop, ignoring execution time of rendering logic and OS scheduler inaccuracies. They treat video and audio as independent streams with assumed perfect clock rates rather than syncing video frames to the hardware audio playback position.

**How to avoid:**
Use the audio stream as the master clock. Before rendering a frame, poll the audio engine (e.g., MiniAudio's playback position) to determine the exact time elapsed, and compute which video frame should be displayed at that moment. Drop frames if the video rendering falls behind.

**Warning signs:**
Noticeable sync drift in longer videos (>1 minute). Video finishing playing while audio is still going, or vice versa.

**Phase to address:**
Player Development (Audio/Video Synchronization Logic)

---

### Pitfall 2: Excessive ANSI Escape Codes & Unbuffered I/O

**What goes wrong:**
The terminal flickers violently, tears frames, and uses 100% CPU to draw, rendering the video unwatchable even at moderate resolutions.

**Why it happens:**
Emitting a full color reset and set code for every single character cell, and writing them via unbuffered `printf` or `putchar`. Terminals struggle with massive I/O throughput.

**How to avoid:**
1. **Delta Updates / Run-Length Encoding:** Only update cells that changed from the previous frame, and use RLE (which is planned) to group cells of the same color.
2. **Buffering:** Construct the entire frame's text buffer in memory and write it in a single `write(STDOUT_FILENO, buffer, size)` call.
3. **Optimize Codes:** Don't emit color codes if the next character shares the same color. 

**Warning signs:**
Visual tearing halfway down the terminal. The terminal emulator becoming completely unresponsive or crashing during playback.

**Phase to address:**
Encoder Development (Format optimization) & Player Development (Renderer)

---

### Pitfall 3: Real-time Transcoding and Color Quantization

**What goes wrong:**
Playback stutters severely on low-end hardware. The CPU overheats because it's converting true-color video frames to the closest 256 or 16 terminal colors on the fly.

**Why it happens:**
Trying to perform spatial downsampling, color quantization, and ASCII character selection during playback.

**How to avoid:**
Do all heavy lifting in a dedicated Ahead-of-Time (AOT) Encoder. The Encoder must pre-compute the exact ANSI strings or exact structural representation for each frame and save them to the custom format (`.ascv`). The player should only be doing I/O, decompression (e.g., zstd), and pushing the buffer to stdout.

**Warning signs:**
High CPU usage during playback. Player requires OpenCV or similar heavy image processing dependencies.

**Phase to address:**
Encoder Development

---

### Pitfall 4: Ignoring Terminal Resize Events

**What goes wrong:**
The user resizes the terminal window during playback, causing the video output to wrap incorrectly, permanently mangling the display until restarted.

**Why it happens:**
Hardcoding the terminal dimensions at startup and failing to handle `SIGWINCH` (Linux/macOS) or `ReadConsoleInput` resize events (Windows).

**How to avoid:**
Trap resize events. When the terminal resizes, the player should immediately clear the screen (`\033[2J`) and either scale the output (if supported) or center it within the new bounds, preventing ugly line wrapping.

**Warning signs:**
Garbage text artifacts appearing on screen resize.

**Phase to address:**
Player Development (Terminal Management)

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Using `system("clear")` | Easy to clear the screen | Causes intense flickering and is slow. | Never for a video player. |
| Hardcoding 80x24 resolution | Simplifies rendering logic | Doesn't utilize modern terminal sizes, poor UX. | MVP only. |
| Assuming 256-color support | Avoids color capability checking | Fails or looks terrible on legacy or strict 16-color terminals. | Initial prototypes. |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| MiniAudio | Initializing in high-latency block-mode. | Initialize with low-latency settings and query `ma_device_get_time()`. |
| FFmpeg (libavformat/codec) | Leaking packets and frames during extraction. | Strictly follow `av_packet_unref` and `av_frame_unref` lifecycles. |
| Windows Console | Standard `\033` escape codes not working. | Call `SetConsoleMode` with `ENABLE_VIRTUAL_TERMINAL_PROCESSING`. |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Small I/O Writes | High system call overhead, tearing. | Buffer entire frames before `write()`. | Resolutions > 80x24 at 30+ FPS. |
| Per-frame heap allocation | Micro-stutters and heap fragmentation. | Pre-allocate a single large double-buffer and reuse it. | Long videos or high frame rates. |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Trusting `.ascv` file dimensions | Buffer overflows when rendering malformed files. | Validate file header metadata against allocated buffers before decoding. |
| Blindly executing embedded ANSI | Arbitrary terminal code execution (e.g. key remapping). | The format should store pixels/characters, not raw, unfiltered escape codes (or filter on playback). |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| No exit prompt or hotkeys | Users stuck in raw terminal mode if they try to `Ctrl+C`. | Catch SIGINT, gracefully restore `termios` before exiting. |
| Leaving cursor visible | Blinking cursor jumping across the screen during playback. | Send `\033[?25l` on start, `\033[?25h` on exit. |

## "Looks Done But Isn't" Checklist

- [ ] **Renderer:** Output displays correctly — verify on multiple terminals (Alacritty, Windows Terminal, Terminal.app).
- [ ] **Audio Sync:** Plays fine for 10 seconds — verify with a 10-minute video to check for drift.
- [ ] **Teardown:** Exits cleanly — verify `Ctrl+C` restores echo and normal terminal behavior.

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Audio sync drift | HIGH | Rewrite main loop to use audio clock as master. |
| Terminal left in broken state | MEDIUM | Users must type `reset`. Add a signal handler for graceful degradation. |
| Performance too slow | HIGH | Redesign file format to store pre-computed ANSI differences. |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| High CPU on playback | Encoder Phase | Playback CPU usage is < 5% on modern machines. |
| Excessive I/O tearing | Player Renderer Phase | High-FPS recording shows no partial frames. |
| Audio Sync Drift | Player Audio Phase | 10-minute test video maintains exact lip sync. |
| Terminal state corrupted | Player Lifecycle Phase | Terminating process randomly leaves terminal usable. |

## Sources

- Common game-loop desync issues (Fix Your Timestep!)
- FFmpeg and MiniAudio integration documentation
- Standard UNIX terminal rendering optimization guides
