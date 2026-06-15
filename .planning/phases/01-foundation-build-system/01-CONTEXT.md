# Phase 1: Foundation & Build System - Context

**Gathered:** 2026-06-15T13:59:54+05:30
**Status:** Ready for planning

<domain>
## Phase Boundary

Set up the core project structure, CMake build system, and define the common `.ascv` binary format specification for the C/C++ cross-platform project.

</domain>

<decisions>
## Implementation Decisions

### Directory Structure
- **D-01:** Monorepo-style layout: Separate `src/encoder`, `src/player`, and shared `include/ascv` directories.

### Dependency Management
- **D-02:** Use CMake FetchContent for everything possible (e.g., zstd, miniaudio) to guarantee versions, but rely on system packages for FFmpeg.

### Binary Format Structure
- **D-03:** Little-endian (native for most modern CPUs) with magic bytes "ASCV".

### Build Strictness
- **D-04:** Moderate strictness (`-Wall -Wextra`, no `-Werror`) for a balance of safety and ease of initial setup.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project Scope
- `.planning/PROJECT.md` — Project definition, core values, and out-of-scope boundaries.
- `.planning/REQUIREMENTS.md` — Active phase requirements.

No external specs — requirements fully captured in decisions above.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- None (brand new project)

### Established Patterns
- C/C++ with CMake
- FetchContent for dependencies

### Integration Points
- `src/encoder` and `src/player` must link against shared includes in `include/ascv` and external libraries like FFmpeg and zstd.

</code_context>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 1-Foundation & Build System*
*Context gathered: 2026-06-15*
