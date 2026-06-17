#include "terminal.hpp"

#include <cstdio>

#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif

namespace ascv {

// ─── Global shutdown flag ─────────────────────────────────────────────────────

volatile sig_atomic_t g_shutdown_requested = 0;

// ─── Signal handler ───────────────────────────────────────────────────────────

static void signal_handler(int /*sig*/) {
    g_shutdown_requested = 1;
}

void setup_signals() {
#ifdef _WIN32
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
#else
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGQUIT, &sa, nullptr);
#endif
}

// ─── TerminalState ────────────────────────────────────────────────────────────

#ifdef _WIN32

TerminalState::TerminalState() {
    hout_ = GetStdHandle(STD_OUTPUT_HANDLE);
    hin_  = GetStdHandle(STD_INPUT_HANDLE);

    if (hout_ == INVALID_HANDLE_VALUE || hin_ == INVALID_HANDLE_VALUE) return;

    // Save current modes
    if (!GetConsoleMode(hout_, &orig_out_mode_)) return;
    if (!GetConsoleMode(hin_,  &orig_in_mode_))  return;

    // Enable ANSI/VT processing on output, disable line/echo on input
    DWORD new_out_mode = orig_out_mode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    DWORD new_in_mode  = orig_in_mode_
                       & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);

    SetConsoleMode(hout_, new_out_mode);
    SetConsoleMode(hin_,  new_in_mode);
    saved_ = true;

    // Enter alternate screen buffer, then hide cursor
    const char* enter = "\x1b[?1049h\x1b[?25l";
    DWORD written = 0;
    WriteConsoleA(hout_, enter, 12, &written, nullptr);
}

TerminalState::~TerminalState() {
    if (!saved_) return;

    // Show cursor and leave alternate screen buffer
    const char* leave = "\x1b[?25h\x1b[?1049l";
    DWORD written = 0;
    WriteConsoleA(hout_, leave, 12, &written, nullptr);

    // Restore original modes
    SetConsoleMode(hout_, orig_out_mode_);
    SetConsoleMode(hin_,  orig_in_mode_);
}

#else // POSIX

TerminalState::TerminalState() {
    if (!isatty(STDOUT_FILENO)) return;

    // Save terminal state
    if (tcgetattr(STDOUT_FILENO, &orig_termios_) != 0) return;
    saved_ = true;

    // Set raw mode: disable canonical mode and local echo
    struct termios raw = orig_termios_;
    raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
    tcsetattr(STDOUT_FILENO, TCSAFLUSH, &raw);

    // Enter alternate screen buffer, then hide cursor
    const char enter[] = "\x1b[?1049h\x1b[?25l";
    write(STDOUT_FILENO, enter, sizeof(enter) - 1);
}

TerminalState::~TerminalState() {
    if (!saved_) return;

    // Show cursor and leave alternate screen buffer (restores original shell content)
    const char leave[] = "\x1b[?25h\x1b[?1049l";
    write(STDOUT_FILENO, leave, sizeof(leave) - 1);

    // Restore original terminal state
    tcsetattr(STDOUT_FILENO, TCSAFLUSH, &orig_termios_);
}

#endif // _WIN32 / POSIX

} // namespace ascv
