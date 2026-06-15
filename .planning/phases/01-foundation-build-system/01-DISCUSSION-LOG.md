# Phase 1: Foundation & Build System - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-15
**Phase:** 1-Foundation & Build System
**Areas discussed:** Directory Structure, Dependency Management, Binary Format Structure, Build Strictness

---

## Directory Structure

| Option | Description | Selected |
|--------|-------------|----------|
| Option 1 | (Recommended) Monorepo-style: Separate src/encoder, src/player, and shared include/ascv | ✓ |
| Option 2 | Flat layout: Everything in src/ | |

**User's choice:** (Recommended) Monorepo-style: Separate src/encoder, src/player, and shared include/ascv
**Notes:** User chose the recommended structure separating encoder, player, and includes.

---

## Dependency Management

| Option | Description | Selected |
|--------|-------------|----------|
| Option 1 | (Recommended) System packages for Linux/Mac (apt/brew) and vcpkg for Windows | |
| Option 2 | Use CMake FetchContent for everything possible (e.g. zstd) to guarantee versions, system packages for FFmpeg | ✓ |
| Option 3 | Use vcpkg for everything across all platforms | |

**User's choice:** Use CMake FetchContent for everything possible (e.g. zstd) to guarantee versions, system packages for FFmpeg
**Notes:** User chose FetchContent with system packages for FFmpeg instead of the recommended OS-specific package managers.

---

## Binary Format Structure

| Option | Description | Selected |
|--------|-------------|----------|
| Option 1 | (Recommended) Little-endian (native for most modern CPUs) with magic bytes "ASCV" | ✓ |
| Option 2 | Big-endian (network byte order) with magic bytes "ASCV" | |

**User's choice:** (Recommended) Little-endian (native for most modern CPUs) with magic bytes "ASCV"
**Notes:** User chose the recommended little-endian structure for the `.ascv` binary format.

---

## Build Strictness

| Option | Description | Selected |
|--------|-------------|----------|
| Option 1 | (Recommended) Strict (-Wall -Wextra -Werror) to catch issues early | |
| Option 2 | Moderate (-Wall -Wextra, no -Werror) for a balance | ✓ |
| Option 3 | Permissive (defaults) to get things running fast | |

**User's choice:** Moderate (-Wall -Wextra, no -Werror) for a balance
**Notes:** User opted for a moderate warning level for the CMake configuration.

---

## the agent's Discretion

None

## Deferred Ideas

None
