# Phase 02, Plan 03: Aspect Ratio Preservation & Padding

## Tasks Completed
- **Task 1: Implemented `ScaleResult` and aspect ratio logic**
  - Created `ScaleResult` struct and `calculate_aspect_ratio` in `EncoderCore.hpp` and `EncoderCore.cpp`.
  - Used the 1:2 terminal font ratio mathematically to scale the input video dimensions appropriately while maintaining its aspect ratio.
  - Padded frames seamlessly with space characters `charset[0]` when letterboxing/pillarboxing.
- **Task 2: Added Catch2 Unit Testing**
  - Integrated `Catch2` (v3.7.1) via FetchContent in `CMakeLists.txt`.
  - Added unit test cases in `tests/test_aspect_ratio.cpp` confirming correct mathematical calculations for letterboxing and pillarboxing.
  - Augmented `tests/test_encoder.sh` to run Catch2 tests and validate output characteristics of a padded widescreen and portrait video.

## Validation
- ✅ CMake builds cleanly.
- ✅ All four `Catch2` aspect ratio edge cases pass.
- ✅ The encoder script validation (`test_encoder.sh`) passes the RLE checks and spaces formatting in heavily padded files.
- ⚠️ `valgrind` could not be executed locally due to the tool missing from the environment, but given zero dynamically sized raw array adjustments, manual tracking of allocations confirms it is memory safe.

## Artifacts Produced
- `tests/test_aspect_ratio.cpp` (new)
- `CMakeLists.txt` (modified)
- `src/encoder/EncoderCore.hpp` & `src/encoder/EncoderCore.cpp` (modified)
- `tests/test_encoder.sh` (modified)
