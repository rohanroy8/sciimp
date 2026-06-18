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

void decompress_rle(const uint8_t* rle_data, size_t rle_size, size_t expected_size, std::vector<char>& out_data) {
    out_data.clear();
    for (size_t i = 0; i + 1 < rle_size; i += 2) {
        uint8_t count = rle_data[i];
        char val = static_cast<char>(rle_data[i + 1]);
        if (out_data.size() + count > expected_size) {
            count = static_cast<uint8_t>(expected_size - out_data.size());
        }
        out_data.insert(out_data.end(), count, val);
        if (out_data.size() >= expected_size) {
            break;
        }
    }
    if (out_data.size() < expected_size) {
        out_data.resize(expected_size, '\0');
    }
}

struct FrameInfo {
    uint64_t offset;
    ascv::FrameType type;
    uint32_t compressed_size;
};

#ifdef _WIN32
#  include <conio.h>
bool get_key_nonblocking(char& ch) {
    if (_kbhit()) {
        int val = _getch();
        if (val == 0 || val == 0xE0) {
            if (_kbhit()) {
                int val2 = _getch();
                if (val2 == 75) {
                    ch = 'L'; // Left Arrow
                    return true;
                } else if (val2 == 77) {
                    ch = 'R'; // Right Arrow
                    return true;
                }
            }
            return false;
        }
        ch = static_cast<char>(val);
        return true;
    }
    return false;
}
#else
bool get_key_nonblocking(char& ch) {
    ssize_t n = ::read(STDIN_FILENO, &ch, 1);
    return (n > 0);
}
#endif

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

    std::vector<FrameInfo> frame_index;
    bool is_seekable = (input != stdin);
    if (is_seekable) {
        long start_pos = ftell(input);
        if (start_pos >= 0) {
            frame_index.reserve(header.frame_count);
            for (uint32_t i = 0; i < header.frame_count; ++i) {
                long offset = ftell(input);
                ascv::FrameHeader f_header;
                if (fread(&f_header, sizeof(f_header), 1, input) != 1) {
                    break;
                }
                FrameInfo info;
                info.offset = static_cast<uint64_t>(offset);
                info.type = f_header.type;
                info.compressed_size = f_header.compressed_size;
                frame_index.push_back(info);

                if (fseek(input, f_header.compressed_size, SEEK_CUR) != 0) {
                    break;
                }
            }
            fseek(input, start_pos, SEEK_SET);
        }
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

    auto start_time = std::chrono::steady_clock::now();
    auto pause_start = std::chrono::steady_clock::now();
    bool paused = false;
    uint32_t current_decoded_frame_idx = 0;

    std::vector<uint8_t> compressed_data;
    std::vector<uint8_t> rle_buffer;
    std::vector<char> decompressed_data;

    while (current_decoded_frame_idx < header.frame_count && !ascv::g_shutdown_requested) {
        char input_key = 0;
        char ch;
        if (term_state.is_raw() && get_key_nonblocking(ch)) {
            if (ch == '\x1b') {
                char ch2, ch3;
                if (get_key_nonblocking(ch2) && ch2 == '[') {
                    if (get_key_nonblocking(ch3)) {
                        if (ch3 == 'C') {
                            input_key = 'R'; // Right Arrow
                        } else if (ch3 == 'D') {
                            input_key = 'L'; // Left Arrow
                        }
                    }
                }
            } else {
#ifdef _WIN32
                if (ch == 'R' || ch == 'L') {
                    input_key = ch;
                } else
#endif
                {
                    input_key = ch;
                }
            }
        }

        if (input_key != 0) {
            if (input_key == 'q') {
                ascv::g_shutdown_requested = 1;
                break;
            } else if (input_key == ' ') {
                paused = !paused;
                if (paused) {
                    if (audio_initialized) {
                        ma_sound_stop(&sound);
                    }
                    pause_start = std::chrono::steady_clock::now();
                } else {
                    if (audio_initialized) {
                        ma_sound_start(&sound);
                    }
                    auto pause_duration = std::chrono::steady_clock::now() - pause_start;
                    start_time += pause_duration;
                }
            } else if (input_key == 'R' || input_key == 'd' || input_key == 'l' ||
                       input_key == 'L' || input_key == 'a' || input_key == 'h') {
                if (is_seekable && frame_index.size() == header.frame_count) {
                    int64_t frame_diff = static_cast<int64_t>(5.0 * (static_cast<double>(header.fps_numerator) / header.fps_denominator));
                    int64_t new_target = static_cast<int64_t>(current_decoded_frame_idx);
                    if (current_decoded_frame_idx > 0) {
                        new_target = static_cast<int64_t>(current_decoded_frame_idx) - 1;
                    }
                    if (input_key == 'R' || input_key == 'd' || input_key == 'l') {
                        new_target += frame_diff;
                    } else {
                        new_target -= frame_diff;
                    }

                    if (new_target < 0) new_target = 0;
                    if (new_target >= static_cast<int64_t>(header.frame_count)) {
                        new_target = static_cast<int64_t>(header.frame_count) - 1;
                    }

                    uint32_t target_idx = static_cast<uint32_t>(new_target);

                    uint32_t keyframe_idx = target_idx;
                    while (keyframe_idx > 0 && frame_index[keyframe_idx].type != ascv::FrameType::I_FRAME) {
                        keyframe_idx--;
                    }

                    if (fseek(input, frame_index[keyframe_idx].offset, SEEK_SET) == 0) {
                        uint32_t idx = keyframe_idx;
                        bool seek_ok = true;
                        while (idx <= target_idx) {
                            ascv::FrameHeader f_header;
                            if (fread(&f_header, sizeof(f_header), 1, input) != 1) {
                                seek_ok = false;
                                break;
                            }

                            compressed_data.resize(f_header.compressed_size);
                            if (fread(compressed_data.data(), 1, f_header.compressed_size, input) != f_header.compressed_size) {
                                seek_ok = false;
                                break;
                            }

                            unsigned long long decompressed_bound = ZSTD_getFrameContentSize(compressed_data.data(), compressed_data.size());
                            if (decompressed_bound == ZSTD_CONTENTSIZE_ERROR || decompressed_bound == ZSTD_CONTENTSIZE_UNKNOWN) {
                                decompressed_bound = 2 * total_cell_bytes + 256;
                            }
                            rle_buffer.resize(decompressed_bound);
                            size_t decompressed_size = ZSTD_decompress(rle_buffer.data(), rle_buffer.size(),
                                                                       compressed_data.data(), compressed_data.size());
                            if (ZSTD_isError(decompressed_size)) {
                                seek_ok = false;
                                break;
                            }
                            rle_buffer.resize(decompressed_size);

                            decompress_rle(rle_buffer.data(), rle_buffer.size(), total_cell_bytes, decompressed_data);

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
                            idx++;
                        }

                        if (seek_ok) {
                            out_buf.clear();
                            out_buf.append("\x1b[H");

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

                            WRITE_RESULT_T written = WRITE_FD(out_buf.data(), out_buf.size());
                            (void)written;

                            current_decoded_frame_idx = target_idx + 1;

                            if (audio_initialized) {
                                double t = target_idx * (static_cast<double>(header.fps_denominator) / header.fps_numerator);
                                ma_sound_seek_to_pcm_frame(&sound, static_cast<ma_uint64>(t * 44100));
                            }

                            if (paused) {
                                pause_start = std::chrono::steady_clock::now();
                            } else {
                                if (!audio_initialized) {
                                    auto now = std::chrono::steady_clock::now();
                                    start_time = now - std::chrono::microseconds(static_cast<int64_t>(current_decoded_frame_idx) * static_cast<int64_t>(frame_duration_us));
                                }
                            }
                        }
                    }
                }
            }
        }

        if (paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

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
                ascv::FrameHeader f_header;
                if (fread(&f_header, sizeof(f_header), 1, input) != 1) {
                    fprintf(stderr, "Error: Unexpected end of file at frame %u (reading header)\n", current_decoded_frame_idx);
                    break;
                }

                compressed_data.resize(f_header.compressed_size);
                if (fread(compressed_data.data(), 1, f_header.compressed_size, input) != f_header.compressed_size) {
                    fprintf(stderr, "Error: Unexpected end of file at frame %u (reading payload)\n", current_decoded_frame_idx);
                    break;
                }

                unsigned long long decompressed_bound = ZSTD_getFrameContentSize(compressed_data.data(), compressed_data.size());
                if (decompressed_bound == ZSTD_CONTENTSIZE_ERROR || decompressed_bound == ZSTD_CONTENTSIZE_UNKNOWN) {
                    decompressed_bound = 2 * total_cell_bytes + 256;
                }
                rle_buffer.resize(decompressed_bound);
                size_t decompressed_size = ZSTD_decompress(rle_buffer.data(), rle_buffer.size(),
                                                           compressed_data.data(), compressed_data.size());
                if (ZSTD_isError(decompressed_size)) {
                    fprintf(stderr, "Error: ZSTD decompression failed at frame %u: %s\n", current_decoded_frame_idx, ZSTD_getErrorName(decompressed_size));
                    break;
                }
                rle_buffer.resize(decompressed_size);

                decompress_rle(rle_buffer.data(), rle_buffer.size(), total_cell_bytes, decompressed_data);

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
                out_buf.clear();
                out_buf.append("\x1b[H");

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

                WRITE_RESULT_T written = WRITE_FD(out_buf.data(), out_buf.size());
                (void)written;
            }
        }

        if (ascv::g_shutdown_requested) break;

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
