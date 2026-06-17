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

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

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

    const uint32_t frame_cells = header.width * header.height;
    ascv::ColorMode color_mode = static_cast<ascv::ColorMode>(header.color_mode);
    size_t S = 1;
    if (color_mode == ascv::ColorMode::MONOCHROME) {
        S = 1;
    } else if (color_mode == ascv::ColorMode::ANSI_16 || color_mode == ascv::ColorMode::ANSI_256) {
        S = 2;
    } else if (color_mode == ascv::ColorMode::RGB_24) {
        S = 4;
    }

    const uint32_t total_cell_bytes = frame_cells * S;

    // Maintain a persistent frame buffer across iterations
    std::vector<char> current_frame(total_cell_bytes);
    if (color_mode == ascv::ColorMode::MONOCHROME) {
        std::fill(current_frame.begin(), current_frame.end(), ' ');
    } else if (color_mode == ascv::ColorMode::ANSI_16 || color_mode == ascv::ColorMode::ANSI_256) {
        for (size_t i = 0; i < frame_cells; ++i) {
            current_frame[i * 2 + 0] = ' ';
            current_frame[i * 2 + 1] = 0;
        }
    } else if (color_mode == ascv::ColorMode::RGB_24) {
        for (size_t i = 0; i < frame_cells; ++i) {
            current_frame[i * 4 + 0] = ' ';
            current_frame[i * 4 + 1] = 0;
            current_frame[i * 4 + 2] = 0;
            current_frame[i * 4 + 3] = 0;
        }
    }

    // Output buffer: dynamic building
    std::string out_buf;
    out_buf.reserve(3 + static_cast<size_t>(header.height) * (header.width * 20 + 1));

    // Hybrid precision timing: frame duration in microseconds
    // frame_duration_us = (1,000,000 * fps_denominator) / fps_numerator
    const uint64_t frame_duration_us =
        (1'000'000ULL * header.fps_denominator) / header.fps_numerator;

    std::string wav_path = "";
    if (argc > 1) {
        wav_path = std::string(argv[1]) + ".wav";
    }

    bool has_audio = false;
    if (!wav_path.empty()) {
        FILE* wav_file = fopen(wav_path.c_str(), "rb");
        if (wav_file) {
            has_audio = true;
            fclose(wav_file);
        }
    }

    ma_engine engine;
    ma_sound sound;
    bool audio_initialized = false;

    if (has_audio) {
        ma_result result = ma_engine_init(nullptr, &engine);
        if (result == MA_SUCCESS) {
            result = ma_sound_init_from_file(&engine, wav_path.c_str(), 0, nullptr, nullptr, &sound);
            if (result == MA_SUCCESS) {
                audio_initialized = true;
                ma_sound_start(&sound);
            } else {
                ma_engine_uninit(&engine);
            }
        }
    }

    const auto start_time = std::chrono::steady_clock::now();
    uint32_t current_decoded_frame_idx = 0;

    while (current_decoded_frame_idx < header.frame_count && !ascv::g_shutdown_requested) {
        uint32_t target_frame = current_decoded_frame_idx;
        if (audio_initialized) {
            float cursor_seconds = 0.0f;
            ma_sound_get_cursor_in_seconds(&sound, &cursor_seconds);
            target_frame = static_cast<uint32_t>(cursor_seconds * (static_cast<double>(header.fps_numerator) / header.fps_denominator));
            if (target_frame >= header.frame_count) {
                target_frame = header.frame_count - 1;
            }
        }

        if (target_frame >= current_decoded_frame_idx) {
            bool frame_updated = false;
            while (current_decoded_frame_idx <= target_frame && current_decoded_frame_idx < header.frame_count) {
                // Read FrameHeader
                ascv::FrameHeader f_header;
                if (fread(&f_header, sizeof(f_header), 1, input) != 1) {
                    fprintf(stderr, "Error: Unexpected end of file at frame %u (reading header)\n", current_decoded_frame_idx);
                    break;
                }

                // Read payload
                std::vector<uint8_t> compressed_data(f_header.compressed_size);
                if (fread(compressed_data.data(), 1, f_header.compressed_size, input) != f_header.compressed_size) {
                    fprintf(stderr, "Error: Unexpected end of file at frame %u (reading payload)\n", current_decoded_frame_idx);
                    break;
                }

                // Zstd decompression
                unsigned long long decompressed_bound = ZSTD_getFrameContentSize(compressed_data.data(), compressed_data.size());
                if (decompressed_bound == ZSTD_CONTENTSIZE_ERROR || decompressed_bound == ZSTD_CONTENTSIZE_UNKNOWN) {
                    decompressed_bound = 2 * total_cell_bytes + 256;
                }
                std::vector<uint8_t> rle_buffer(decompressed_bound);
                size_t decompressed_size = ZSTD_decompress(rle_buffer.data(), rle_buffer.size(),
                                                           compressed_data.data(), compressed_data.size());
                if (ZSTD_isError(decompressed_size)) {
                    fprintf(stderr, "Error: ZSTD decompression failed at frame %u: %s\n", current_decoded_frame_idx, ZSTD_getErrorName(decompressed_size));
                    break;
                }
                rle_buffer.resize(decompressed_size);

                // RLE decompression
                std::vector<char> decompressed_data = decompress_rle(rle_buffer.data(), rle_buffer.size(), total_cell_bytes);

                // Apply decompressed data to current_frame
                if (f_header.type == ascv::FrameType::I_FRAME) {
                    std::copy(decompressed_data.begin(), decompressed_data.end(), current_frame.begin());
                } else {
                    for (size_t i = 0; i < frame_cells; ++i) {
                        if (decompressed_data[i * S] != '\0') {
                            for (size_t b = 0; b < S; ++b) {
                                current_frame[i * S + b] = decompressed_data[i * S + b];
                            }
                        }
                    }
                }
                current_decoded_frame_idx++;
                frame_updated = true;
            }

            if (frame_updated && !ascv::g_shutdown_requested) {
                // Build output buffer
                out_buf.clear();
                out_buf.append("\x1b[H"); // Cursor home

                if (color_mode == ascv::ColorMode::MONOCHROME) {
                    for (uint16_t row = 0; row < header.height; ++row) {
                        const char* row_start = current_frame.data() + row * header.width;
                        out_buf.append(row_start, header.width);
                        if (row + 1 < header.height) out_buf.push_back('\n');
                    }
                } else if (color_mode == ascv::ColorMode::ANSI_16 || color_mode == ascv::ColorMode::ANSI_256) {
                    int last_fg_idx = -1;
                    for (uint16_t row = 0; row < header.height; ++row) {
                        for (uint16_t col = 0; col < header.width; ++col) {
                            size_t idx_in_frame = row * header.width + col;
                            char ch = current_frame[idx_in_frame * 2 + 0];
                            uint8_t color_idx = static_cast<uint8_t>(current_frame[idx_in_frame * 2 + 1]);
                            if (static_cast<int>(color_idx) != last_fg_idx) {
                                char code_buf[32];
                                int len = snprintf(code_buf, sizeof(code_buf), "\x1b[38;5;%dm", static_cast<int>(color_idx));
                                out_buf.append(code_buf, len);
                                last_fg_idx = color_idx;
                            }
                            out_buf.push_back(ch);
                        }
                        if (row + 1 < header.height) out_buf.push_back('\n');
                    }
                    out_buf.append("\x1b[0m");
                } else if (color_mode == ascv::ColorMode::RGB_24) {
                    int last_r = -1, last_g = -1, last_b = -1;
                    for (uint16_t row = 0; row < header.height; ++row) {
                        for (uint16_t col = 0; col < header.width; ++col) {
                            size_t idx_in_frame = row * header.width + col;
                            char ch = current_frame[idx_in_frame * 4 + 0];
                            uint8_t r = static_cast<uint8_t>(current_frame[idx_in_frame * 4 + 1]);
                            uint8_t g = static_cast<uint8_t>(current_frame[idx_in_frame * 4 + 2]);
                            uint8_t b = static_cast<uint8_t>(current_frame[idx_in_frame * 4 + 3]);
                            if (static_cast<int>(r) != last_r || static_cast<int>(g) != last_g || static_cast<int>(b) != last_b) {
                                char code_buf[48];
                                int len = snprintf(code_buf, sizeof(code_buf), "\x1b[38;2;%d;%d;%dm", static_cast<int>(r), static_cast<int>(g), static_cast<int>(b));
                                out_buf.append(code_buf, len);
                                last_r = r;
                                last_g = g;
                                last_b = b;
                            }
                            out_buf.push_back(ch);
                        }
                        if (row + 1 < header.height) out_buf.push_back('\n');
                    }
                    out_buf.append("\x1b[0m");
                }

                // Write entire frame in one syscall
                WRITE_RESULT_T written = WRITE_FD(out_buf.data(), out_buf.size());
                (void)written;
            }
        }

        if (ascv::g_shutdown_requested) break;

        // Pacing
        if (audio_initialized) {
            double next_frame_time = current_decoded_frame_idx * (static_cast<double>(header.fps_denominator) / header.fps_numerator);
            float current_audio_time = 0.0f;
            ma_sound_get_cursor_in_seconds(&sound, &current_audio_time);
            double remaining_seconds = next_frame_time - current_audio_time;
            if (remaining_seconds > 0.002) {
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>((remaining_seconds - 0.002) * 1000)));
            }
            while (!ascv::g_shutdown_requested) {
                if (ma_sound_at_end(&sound)) {
                    break;
                }
                float current_time = 0.0f;
                ma_sound_get_cursor_in_seconds(&sound, &current_time);
                if (current_time >= next_frame_time) {
                    break;
                }
                std::this_thread::yield();
            }
        } else {
            // Hybrid precision timing fallback
            const auto target_time = start_time +
                std::chrono::microseconds(static_cast<int64_t>(current_decoded_frame_idx) *
                                          static_cast<int64_t>(frame_duration_us));
            const auto now = std::chrono::steady_clock::now();
            const auto remaining = target_time - now;
            if (remaining > std::chrono::milliseconds(2)) {
                std::this_thread::sleep_for(remaining - std::chrono::milliseconds(2));
            }
            while (std::chrono::steady_clock::now() < target_time && !ascv::g_shutdown_requested) {
                // Busy-wait
            }
        }
    }

    if (color_mode != ascv::ColorMode::MONOCHROME) {
        WRITE_FD("\x1b[0m", 4);
    }

    if (audio_initialized) {
        ma_sound_stop(&sound);
        ma_sound_uninit(&sound);
        ma_engine_uninit(&engine);
    }

    if (input != stdin) fclose(input);
    // TerminalState destructor restores cursor and terminal mode automatically
    return 0;
}
