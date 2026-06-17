#pragma once

// ascv/terminal.hpp — Terminal mode management and signal handling
// Handles raw mode, cursor visibility, and graceful shutdown signals.

#ifdef _WIN32
#  include <windows.h>
#else
#  include <termios.h>
#  include <csignal>
#endif

#include <cstdint>

namespace ascv {

// Saves the original terminal state at construction, restores it at destruction.
// Also hides the cursor on entry and shows it on exit (POSIX and Windows).
class TerminalState {
public:
    TerminalState();
    ~TerminalState();

    // Non-copyable, non-movable
    TerminalState(const TerminalState&) = delete;
    TerminalState& operator=(const TerminalState&) = delete;

private:
    bool saved_{false};

#ifdef _WIN32
    DWORD orig_out_mode_{0};
    DWORD orig_in_mode_{0};
    HANDLE hout_{INVALID_HANDLE_VALUE};
    HANDLE hin_{INVALID_HANDLE_VALUE};
#else
    struct termios orig_termios_{};
#endif
};

// Global flag set by signal handlers. Checked each frame iteration.
extern volatile sig_atomic_t g_shutdown_requested;

// Install handlers for SIGINT, SIGTERM (and SIGQUIT on POSIX).
void setup_signals();

} // namespace ascv
