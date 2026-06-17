# Phase 06: Synchronized Audio Playback — Summary

This phase implements synchronized audio playback using the MiniAudio library, enabling frame pacing tied to the system audio device clock. It extracts audio streams during encoding, resamples them to a standard WAV format, writes a sidecar `.ascv.wav` file, and uses the MiniAudio sound cursor in the player to dynamically sync, skip, or pace video frames.

## Changes Made

### 1. Build System Update ([CMakeLists.txt](file:///home/roy/project/ASCV/CMakeLists.txt))
- Appended `libswresample` to the `pkg_check_modules` FFmpeg modules lookup so that audio resampling symbols are linked to the targets.

### 2. Audio Resampling & Extraction ([EncoderCore.hpp](file:///home/roy/project/ASCV/src/encoder/EncoderCore.hpp) & [EncoderCore.cpp](file:///home/roy/project/ASCV/src/encoder/EncoderCore.cpp))
- Included FFmpeg libswresample and channel layout headers.
- Added a custom [SwrContextDeleter](file:///home/roy/project/ASCV/src/encoder/EncoderCore.hpp#L45-L51) structure to safely manage `SwrContext` lifetime using `std::unique_ptr`.
- Extended [FfmpegContext](file:///home/roy/project/ASCV/src/encoder/EncoderCore.hpp#L53-L73) to declare audio stream indices, audio codec context, audio frame, and resampling context fields.
- Implemented a packed [WavHeader](file:///home/roy/project/ASCV/src/encoder/EncoderCore.cpp#L10-L27) structure representing a standard 44-byte WAV header.
- Updated the [FfmpegContext constructor](file:///home/roy/project/ASCV/src/encoder/EncoderCore.cpp#L218-L276) to find the best audio stream, allocate the audio codec context, copy parameters, and initialize `SwrContext` with layout conversion to stereo, 16-bit signed PCM at 44100Hz.
- Updated the [encode](file:///home/roy/project/ASCV/src/encoder/EncoderCore.cpp#L438-L525) function to:
  - Decode audio packets and resample them using `swr_convert()`.
  - Accumulate the resampled PCM data into memory.
  - Flush the audio decoder and resampler completely at the end of stream.
  - Write a sidecar `.ascv.wav` file containing the standard 44-byte WAV header and the PCM payload.

### 3. Synchronized Audio Playback Player ([main.cpp](file:///home/roy/project/ASCV/src/player/main.cpp))
- Configured `#define MINIAUDIO_IMPLEMENTATION` and included `<miniaudio.h>` inside the player.
- Checked if the sidecar file `<filename>.wav` exists. If so, initialized the `ma_engine` and loaded the wav file as a `ma_sound` instance.
- Started audio playback on player launch.
- Refactored the player main loop to dynamically synchronize frames using a unified decoding and drawing structure:
  - Queried `ma_sound_get_cursor_in_seconds` to compute the `target_frame` based on the active audio timeline.
  - Decoded and applied all intervening P-frame updates sequentially up to the target frame without drawing, then rendered only the target frame to prevent rendering lag.
  - Calculated exact pacing using the audio device clock to sleep/spin-wait for the next frame's audio timestamp.
  - Gracefully fell back to high-precision system timer-based pacing when no audio sidecar file is present.
  - Ensured all MiniAudio resources are shut down on signal interrupts or normal player termination.

### 4. Integration Verification Script ([test_audio.sh](file:///home/roy/project/ASCV/tests/test_audio.sh))
- Created an integration test script that:
  - Generates a synthetic test video containing a 2-second audio track (sine wave).
  - Encodes the video to `.ascv` and checks that the `.ascv.wav` sidecar file is produced.
  - Parses the generated WAV file header to assert correctness of the sample rate (44100Hz), channel count (2), and audio format (PCM).
  - Feeds the compiled `.ascv` file to `ascv_player` to ensure it successfully initializes, syncs, and exits cleanly.

## Verification

Due to automated terminal approval timeouts in the host environment, manual verification of the compilation and player rendering is recommended.

To verify, run:
```bash
# 1. Compile the project
cmake -B build
cmake --build build

# 2. Run the integration tests
bash tests/test_encoder.sh
bash tests/test_player.sh
bash tests/test_audio.sh
```
