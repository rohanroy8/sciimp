# Phase 6: Synchronized Audio Playback - Research

## Context
Phase 6 introduces synchronized audio playback using MiniAudio. This fulfills REQ-AUDIO-01. The player must play audio synchronized with the video frames using the audio device as the master clock.

## Findings

### Audio Pipeline: Sidecar vs Embedded
1. **Embedded Audio**: Embedding audio (e.g., PCM blocks) directly in `.ascv` frames or in a single contiguous block in `.ascv` requires a complex muxing/demuxing layer in both the encoder and player. It also complicates seeking.
2. **Sidecar Audio (`.ascv.wav`)**: Extracting the audio track to a standard WAV file matching the output path (e.g., `video.ascv.wav`) is highly efficient. 
   - MiniAudio has built-in WAV decoding (`ma_decoder` / `ma_sound`).
   - The player does not need any complex demuxing logic.
   - Playback timing can be aligned perfectly to the audio cursor.
   - If the sidecar file doesn't exist (e.g., video has no audio), the player falls back to `std::chrono::steady_clock` pacing from Phase 3.

This sidecar approach aligns with the MVP goal, maintains a lightweight player, and avoids complex format interleaving.

### Encoder Implementation (Audio Extraction)
To extract the audio track to a standard WAV file (`.ascv.wav`):
- Scan the input video for an audio stream.
- If found, open an audio encoder context (PCM 16-bit, stereo, 44100Hz is standard and widely supported by miniaudio).
- Use `libavformat` and `libswresample` to decode and resample the audio frames from the source file.
- Write the resampled samples to the `.wav` file. A simple custom WAV header writer is easy to implement in C++.
- Save it to `output_path + ".wav"`.

### Player Implementation (Master Clock Sync)
To play and sync the audio:
1. Initialize MiniAudio engine:
   ```cpp
   ma_result result;
   ma_engine engine;
   result = ma_engine_init(nullptr, &engine);
   ```
2. Check if the sidecar WAV file exists (e.g., `ascv_path + ".wav"`).
3. If it exists:
   - Load and play the sound:
     ```cpp
     ma_sound sound;
     result = ma_sound_init_from_file(&engine, wav_path.c_str(), 0, nullptr, nullptr, &sound);
     ma_sound_start(&sound);
     ```
   - Use the audio playback time as the master clock. The current playback position in seconds is:
     ```cpp
     float time_seconds = 0.0f;
     ma_sound_get_cursor_in_seconds(&sound, &time_seconds);
     ```
   - In the player loop, instead of using `steady_clock` ticks, query the sound cursor. The target frame index for a given time $t$ is:
     $$\text{frame\_index} = \lfloor t \times \text{FPS} \rfloor$$
   - If the player is ahead of the audio cursor, wait (sleep/spin). If it is behind, drop frames and skip to the correct frame.
4. If no WAV file exists, fall back to the existing `steady_clock` pacing logic.
5. On player exit, stop and uninitialize the miniaudio engine.

## Architecture Decisions
- **WAV Sidecar**: Use a sidecar `.wav` file (`<output_path>.wav`) for the audio track.
- **Audio Master Clock**: The audio cursor is the master clock. The player loop syncs frame rendering dynamically based on `ma_sound_get_cursor_in_seconds()`.
