#include "ascv/format.hpp"
#include "terminal.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#  include <windows.h>
#  define WRITE_FD(buf, len) _write(1, (buf), static_cast<unsigned int>(len))
#else
#  include <unistd.h>
#  define WRITE_FD(buf, len) write(STDOUT_FILENO, (buf), (len))
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

        // Rows
        for (uint16_t row = 0; row < header.height; ++row) {
            const char* row_start = raw_frame.data() + row * header.width;
            memcpy(&out_buf[pos], row_start, header.width);
            pos += header.width;
            out_buf[pos++] = '\n';
        }

        // Write entire frame in one syscall
        ssize_t written = WRITE_FD(out_buf.data(), pos);
        (void)written;
    }

    if (input != stdin) fclose(input);
    // TerminalState destructor restores cursor and terminal mode automatically
    return 0;
}
