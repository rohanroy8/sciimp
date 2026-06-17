---
status: testing
phase: 03-minimal-player
source: [03-VERIFICATION.md]
started: "2026-06-17T07:25:00Z"
updated: "2026-06-17T07:25:00Z"
---

## Current Test

number: 1
name: Visual frame rendering at correct FPS
expected: |
  Running ascv_player on the test .ascv file renders ASCII frames at ~10fps for ~1 second.
  Cursor returns to top of frame between frames (no scrolling). Completes cleanly.
awaiting: user response

## Tests

### 1. Visual frame rendering at correct FPS

expected: |
  `./build/src/player/ascv_player /tmp/ascv_test_output.ascv` renders frames in the terminal
  at approximately 10fps for ~1 second, each frame overwriting the previous using cursor-home.
  No scrolling. Player exits cleanly after all frames.
result: [pending]

### 2. Ctrl+C restores terminal completely

expected: |
  During playback of a longer video, pressing Ctrl+C stops the player immediately.
  After stopping: cursor is visible, terminal echo is restored, shell is fully functional.
  No corrupted terminal state (no need to run `reset`).
result: [pending]

## Summary

total: 2
passed: 0
issues: 0
pending: 2
skipped: 0
blocked: 0

## Gaps
