# Phase 7: Seeking/Scrubbing - Research

## Context
Phase 7 adds seeking and scrubbing capabilities to the ASCV player. This satisfies REQ-AUDIO-02. The player must support seeking forward and backward in the video, finding the nearest preceding keyframe (I-frame), applying intermediate delta P-frames, and re-establishing perfect synchronization with the audio device track cursor.

## Findings

### Non-blocking Keyboard Input
To read keyboard controls interactively without blocking the playback loop:
- **POSIX (Linux/macOS)**: Configure `STDIN_FILENO` termios settings with `VMIN = 0` and `VTIME = 0`. This causes `read(STDIN_FILENO, &ch, 1)` to return immediately with `0` if no keys are pressed.
- **Windows**: Use `_kbhit()` and `_getch()` from `<conio.h>`.

We will implement a cross-platform helper: `bool get_key_nonblocking(char& ch)`.

### Controls
We will support the following keyboard controls:
- `Right Arrow` or `d` / `l`: Seek forward 5 seconds.
- `Left Arrow` or `a` / `h`: Seek backward 5 seconds.
- `Space`: Pause / Resume.
- `q`: Quit playback.

### Frame Offset Index
Since `.ascv` contains variable-sized zstd-compressed frames, we cannot seek directly to a frame index by calculating a file offset in O(1).
Instead, on player startup, we can perform a quick sequential scan of the file to build an in-memory index:
```cpp
struct FrameInfo {
    uint64_t offset;
    FrameType type;
    uint32_t compressed_size;
};
std::vector<FrameInfo> frame_index;
```
For a 10-minute video at 30 FPS (18,000 frames), the index contains 18,000 elements, consuming ~400 KB of RAM, and takes less than 1ms to scan on startup.

### Seeking Logic
To seek to a target frame index `target_frame`:
1. Find the nearest preceding keyframe (I-frame):
   ```cpp
   uint32_t keyframe_idx = target_frame;
   while (keyframe_idx > 0 && frame_index[keyframe_idx].type != FrameType::I_FRAME) {
       keyframe_idx--;
   }
   ```
2. Seek the file stream to `frame_index[keyframe_idx].offset`.
3. Decode the I-frame to initialize the persistent frame buffer.
4. Sequentially decode all P-frames from `keyframe_idx + 1` up to `target_frame` to accumulate state in the buffer (do not render these intermediate frames to STDOUT).
5. Render the target frame.
6. Synchronize the Audio Clock:
   - Calculate target timestamp: $t = \text{target\_frame} \times \frac{\text{fps\_denominator}}{\text{fps\_numerator}}$.
   - Call MiniAudio `ma_sound_seek_to_pcm_frame(&sound, static_cast<ma_uint64>(t * 44100))` to seek the audio track cursor.
7. Update `current_decoded_frame_idx` to `target_frame`.

## Architecture Decisions
- **Sidecar Audio Seek**: MiniAudio supports seeking via `ma_sound_seek_to_pcm_frame()`. We will seek the sound to align with the selected video timestamp.
- **Pause State**: Implement a `bool paused` flag. When paused, stop the audio playback (`ma_sound_stop()`) and pause the rendering clock loop. On resume, start the audio playback (`ma_sound_start()`).
