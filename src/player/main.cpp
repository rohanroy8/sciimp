#include "ascv/format.hpp"
#include "terminal.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#  include <windows.h>
#  define WRITE_FD(buf, len) _write(1, (buf), static_cast<unsigned int>(len))
#  define WRITE_RESULT_T int
#else
#  include <unistd.h>
#  define WRITE_FD(buf, len) write(STDOUT_FILENO, (buf), (len))
#  define WRITE_RESULT_T ssize_t
#endif

int main(int argc, char* argv[]) {
    // Install signal handlers and enter raw terminal mode
    ascv::setup_signals();
    ascv::TerminalState term_state;

    FILE* input = nullptr;

    if (argc > 1) {
        input = fopen(argv[1], "rb");
        if (!input) {
            fprintf(stderr, "Error: Cannot open '%s'\n", argv[1]);
            return 1;
        }
    } else {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        input = stdin;
    }

    // Read and validate header
    ascv::FileHeader header;
    if (fread(&header, sizeof(header), 1, input) != 1) {
        fprintf(stderr, "Error: Failed to read ASCV header\n");
        if (input != stdin) fclose(input);
        return 1;
    }

    if (!ascv::validate_magic(header)) {
        fprintf(stderr, "Error: Invalid ASCV magic bytes\n");
        if (input != stdin) fclose(input);
        return 1;
    }

    const uint32_t frame_bytes = header.width * header.height;

    // Pre-allocate raw frame buffer (no newlines — added during rendering)
    std::vector<char> raw_frame(frame_bytes);

    // Output buffer: ESC[H + height rows, each row = width bytes + '\n'
    const size_t out_size = 3 + static_cast<size_t>(header.height) * (header.width + 1);
    std::string out_buf;
    out_buf.resize(out_size);

    // Hybrid precision timing: frame duration in microseconds
    // frame_duration_us = (1,000,000 * fps_denominator) / fps_numerator
    const uint64_t frame_duration_us =
        (1'000'000ULL * header.fps_denominator) / header.fps_numerator;

    const auto start_time = std::chrono::steady_clock::now();

    for (uint32_t f = 0; f < header.frame_count; ++f) {
        // Check for shutdown signal (Ctrl+C / SIGTERM / SIGQUIT)
        if (ascv::g_shutdown_requested) break;

        if (fread(raw_frame.data(), 1, frame_bytes, input) != frame_bytes) {
            fprintf(stderr, "Error: Unexpected end of file at frame %u\n", f);
            if (input != stdin) fclose(input);
            return 1;
        }

        // Build output buffer
        size_t pos = 0;
        // Cursor home
        out_buf[pos++] = '\x1b';
        out_buf[pos++] = '[';
        out_buf[pos++] = 'H';

        // Rows — no '\n' after the last row to prevent terminal scroll at bottom edge
        for (uint16_t row = 0; row < header.height; ++row) {
            const char* row_start = raw_frame.data() + row * header.width;
            memcpy(&out_buf[pos], row_start, header.width);
            pos += header.width;
            if (row + 1 < header.height) out_buf[pos++] = '\n';
        }

        // Write entire frame in one syscall
        WRITE_RESULT_T written = WRITE_FD(out_buf.data(), pos);
        (void)written;

        // Hybrid precision timing
        // Calculate absolute target time for this frame
        const auto target_time = start_time +
            std::chrono::microseconds(static_cast<int64_t>(f + 1) *
                                      static_cast<int64_t>(frame_duration_us));

        const auto now = std::chrono::steady_clock::now();
        const auto remaining = target_time - now;

        // Sleep if > 2ms remaining to yield CPU, then spin-wait for precision
        if (remaining > std::chrono::milliseconds(2)) {
            std::this_thread::sleep_for(remaining - std::chrono::milliseconds(2));
        }

        // Spin-wait for the last 2ms (or the full remaining duration if < 2ms)
        while (std::chrono::steady_clock::now() < target_time) {
            // Busy-wait for sub-millisecond precision
        }
    }

    if (input != stdin) fclose(input);
    // TerminalState destructor restores cursor and terminal mode automatically
    return 0;
}
