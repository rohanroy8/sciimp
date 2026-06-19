# Phase 11: Palette Compression — Summary

This phase introduced embedded color palettes for high-fidelity frames, avoiding the repetition of 24-bit TrueColor ANSI codes.

## Changes Made
- Extracted and quantized dynamic frame palettes.
- Embedded color palettes inside the `.ascv` binary.
- Updated player to map indexed colors back into full ANSI TrueColor strings during drawing.
