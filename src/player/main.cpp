#include "ascv/format.hpp"
#include "terminal.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <zstd.h>

namespace {

std::vector<char> decompress_rle(const uint8_t* rle_data, size_t rle_size, size_t expected_size) {
    std::vector<char> data;
    data.reserve(expected_size);
    for (size_t i = 0; i + 1 < rle_size; i += 2) {
        uint8_t count = rle_data[i];
        char val = static_cast<char>(rle_data[i + 1]);
        if (data.size() + count > expected_size) {
            count = static_cast<uint8_t>(expected_size - data.size());
        }
        data.insert(data.end(), count, val);
        if (data.size() >= expected_size) {
            break;
        }
    }
    if (data.size() < expected_size) {
        data.resize(expected_size, '\0');
    }
    return data;
}

} // namespace

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

    // Maintain a persistent frame buffer across iterations
    std::vector<char> current_frame(frame_bytes, ' ');

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

        // Read FrameHeader
        ascv::FrameHeader f_header;
        if (fread(&f_header, sizeof(f_header), 1, input) != 1) {
            fprintf(stderr, "Error: Unexpected end of file at frame %u (reading header)\n", f);
            if (input != stdin) fclose(input);
            return 1;
        }

        // Read payload
        std::vector<uint8_t> compressed_data(f_header.compressed_size);
        if (fread(compressed_data.data(), 1, f_header.compressed_size, input) != f_header.compressed_size) {
            fprintf(stderr, "Error: Unexpected end of file at frame %u (reading payload)\n", f);
            if (input != stdin) fclose(input);
            return 1;
        }

        // Zstd decompression
        unsigned long long decompressed_bound = ZSTD_getFrameContentSize(compressed_data.data(), compressed_data.size());
        if (decompressed_bound == ZSTD_CONTENTSIZE_ERROR || decompressed_bound == ZSTD_CONTENTSIZE_UNKNOWN) {
            decompressed_bound = 2 * frame_bytes + 256;
        }
        std::vector<uint8_t> rle_buffer(decompressed_bound);
        size_t decompressed_size = ZSTD_decompress(rle_buffer.data(), rle_buffer.size(),
                                                   compressed_data.data(), compressed_data.size());
        if (ZSTD_isError(decompressed_size)) {
            fprintf(stderr, "Error: ZSTD decompression failed at frame %u: %s\n", f, ZSTD_getErrorName(decompressed_size));
            if (input != stdin) fclose(input);
            return 1;
        }
        rle_buffer.resize(decompressed_size);

        // RLE decompression
        std::vector<char> decompressed_data = decompress_rle(rle_buffer.data(), rle_buffer.size(), frame_bytes);

        // Apply decompressed data to current_frame
        if (f_header.type == ascv::FrameType::I_FRAME) {
            std::copy(decompressed_data.begin(), decompressed_data.end(), current_frame.begin());
        } else {
            for (size_t i = 0; i < frame_bytes; ++i) {
                if (decompressed_data[i] != '\0') {
                    current_frame[i] = decompressed_data[i];
                }
            }
        }

        // Build output buffer
        size_t pos = 0;
        // Cursor home
        out_buf[pos++] = '\x1b';
        out_buf[pos++] = '[';
        out_buf[pos++] = 'H';

        // Rows — no '\n' after the last row to prevent terminal scroll at bottom edge
        for (uint16_t row = 0; row < header.height; ++row) {
            const char* row_start = current_frame.data() + row * header.width;
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
