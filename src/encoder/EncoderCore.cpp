#include "EncoderCore.hpp"
#include "ascv/format.hpp"
#include <zstd.h>
#include <zdict.h>
#include <stdexcept>
#include <fstream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <unordered_map>

namespace ascv::encoder {

namespace {

#pragma pack(push, 1)
struct WavHeader {
    char chunk_id[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunk_size = 0; // 36 + subchunk2_size
    char format[4] = {'W', 'A', 'V', 'E'};
    char subchunk1_id[4] = {'f', 'm', 't', ' '};
    uint32_t subchunk1_size = 16;
    uint16_t audio_format = 1; // PCM = 1
    uint16_t num_channels = 2; // Stereo
    uint32_t sample_rate = 44100;
    uint32_t byte_rate = 44100 * 2 * 2;
    uint16_t block_align = 2 * 2;
    uint16_t bits_per_sample = 16;
    char subchunk2_id[4] = {'d', 'a', 't', 'a'};
    uint32_t subchunk2_size = 0;
};
#pragma pack(pop)

struct RGB {
    uint8_t r, g, b;
};

RGB get_ansi_color_rgb(int index) {
    if (index < 16) {
        static const RGB std16[16] = {
            {0, 0, 0}, {128, 0, 0}, {0, 128, 0}, {128, 128, 0},
            {0, 0, 128}, {128, 0, 128}, {0, 128, 128}, {192, 192, 192},
            {128, 128, 128}, {255, 0, 0}, {0, 255, 0}, {255, 255, 0},
            {0, 0, 255}, {255, 0, 255}, {0, 255, 255}, {255, 255, 255}
        };
        return std16[index];
    } else if (index < 232) {
        int r = (index - 16) / 36;
        int g = ((index - 16) / 6) % 6;
        int b = (index - 16) % 6;
        auto step = [](int val) -> uint8_t {
            return val == 0 ? 0 : 55 + val * 40;
        };
        return {step(r), step(g), step(b)};
    } else {
        uint8_t val = 8 + (index - 232) * 10;
        return {val, val, val};
    }
}

uint8_t rgb_to_ansi16(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t best_idx = 0;
    int min_dist = 10000000;
    for (int i = 0; i < 16; ++i) {
        RGB p = get_ansi_color_rgb(i);
        int dr = static_cast<int>(r) - p.r;
        int dg = static_cast<int>(g) - p.g;
        int db = static_cast<int>(b) - p.b;
        int dist = dr*dr + dg*dg + db*db;
        if (dist < min_dist) {
            min_dist = dist;
            best_idx = i;
        }
    }
    return best_idx;
}

uint8_t rgb_to_ansi256(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t best_idx = 0;
    int min_dist = 10000000;
    for (int i = 0; i < 256; ++i) {
        RGB p = get_ansi_color_rgb(i);
        int dr = static_cast<int>(r) - p.r;
        int dg = static_cast<int>(g) - p.g;
        int db = static_cast<int>(b) - p.b;
        int dist = dr*dr + dg*dg + db*db;
        if (dist < min_dist) {
            min_dist = dist;
            best_idx = i;
        }
    }
    return best_idx;
}

std::vector<uint8_t> compress_rle(const std::vector<char>& data) {
    std::vector<uint8_t> rle;
    if (data.empty()) {
        return rle;
    }
    uint8_t current_val = static_cast<uint8_t>(data[0]);
    size_t count = 1;
    for (size_t i = 1; i < data.size(); ++i) {
        uint8_t val = static_cast<uint8_t>(data[i]);
        if (val == current_val) {
            count++;
        } else {
            while (count > 255) {
                rle.push_back(255);
                rle.push_back(current_val);
                count -= 255;
            }
            rle.push_back(static_cast<uint8_t>(count));
            rle.push_back(current_val);
            current_val = val;
            count = 1;
        }
    }
    while (count > 255) {
        rle.push_back(255);
        rle.push_back(current_val);
        count -= 255;
    }
    rle.push_back(static_cast<uint8_t>(count));
    rle.push_back(current_val);
    return rle;
}

} // namespace

FfmpegContext::FfmpegContext(const std::string& input_path) {
    AVFormatContext* format_ctx_raw = nullptr;
    if (avformat_open_input(&format_ctx_raw, input_path.c_str(), nullptr, nullptr) < 0) {
        throw std::runtime_error("Could not open input file");
    }
    fmt_ctx.reset(format_ctx_raw);

    if (avformat_find_stream_info(fmt_ctx.get(), nullptr) < 0) {
        throw std::runtime_error("Could not find stream info");
    }

    const AVCodec* decoder = nullptr;
    video_stream_index = av_find_best_stream(fmt_ctx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (video_stream_index < 0) {
        throw std::runtime_error("Could not find a video stream");
    }

    AVStream* video_stream = fmt_ctx->streams[video_stream_index];

    AVCodecContext* codec_ctx_raw = avcodec_alloc_context3(decoder);
    if (!codec_ctx_raw) {
        throw std::runtime_error("Failed to allocate codec context");
    }
    codec_ctx.reset(codec_ctx_raw);

    if (avcodec_parameters_to_context(codec_ctx.get(), video_stream->codecpar) < 0) {
        throw std::runtime_error("Failed to copy codec parameters to context");
    }

    if (avcodec_open2(codec_ctx.get(), decoder, nullptr) < 0) {
        throw std::runtime_error("Failed to open codec");
    }

    frame.reset(av_frame_alloc());
    scaled_frame.reset(av_frame_alloc());
    packet.reset(av_packet_alloc());

    if (!frame || !scaled_frame || !packet) {
        throw std::runtime_error("Failed to allocate FFmpeg structs");
    }

    if (video_stream->avg_frame_rate.num > 0) {
        fps_num = video_stream->avg_frame_rate.num;
        fps_den = video_stream->avg_frame_rate.den;
    } else {
        fps_num = video_stream->r_frame_rate.num;
        fps_den = video_stream->r_frame_rate.den;
    }

    if (fps_den == 0) {
        throw std::runtime_error("Invalid framerate from video stream");
    }

    // Audio stream lookup
    const AVCodec* audio_decoder = nullptr;
    audio_stream_index = av_find_best_stream(fmt_ctx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, &audio_decoder, 0);
    if (audio_stream_index >= 0) {
        AVStream* audio_stream = fmt_ctx->streams[audio_stream_index];
        AVCodecContext* audio_codec_ctx_raw = avcodec_alloc_context3(audio_decoder);
        if (!audio_codec_ctx_raw) {
            throw std::runtime_error("Failed to allocate audio codec context");
        }
        audio_codec_ctx.reset(audio_codec_ctx_raw);

        if (avcodec_parameters_to_context(audio_codec_ctx.get(), audio_stream->codecpar) < 0) {
            throw std::runtime_error("Failed to copy audio codec parameters to context");
        }

        if (avcodec_open2(audio_codec_ctx.get(), audio_decoder, nullptr) < 0) {
            throw std::runtime_error("Failed to open audio codec");
        }

        audio_frame.reset(av_frame_alloc());
        if (!audio_frame) {
            throw std::runtime_error("Failed to allocate audio frame");
        }

        // Initialize SwrContext
        SwrContext* swr_ctx = nullptr;
        AVChannelLayout in_ch_layout;
        if (audio_codec_ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
            av_channel_layout_default(&in_ch_layout, audio_codec_ctx->ch_layout.nb_channels);
        } else {
            av_channel_layout_copy(&in_ch_layout, &audio_codec_ctx->ch_layout);
        }
        AVChannelLayout out_ch_layout;
        av_channel_layout_default(&out_ch_layout, 2);

        int ret = swr_alloc_set_opts2(
            &swr_ctx,
            &out_ch_layout,
            AV_SAMPLE_FMT_S16, // out sample format: 16-bit signed PCM
            44100,             // out sample rate
            &in_ch_layout,
            audio_codec_ctx->sample_fmt,
            audio_codec_ctx->sample_rate,
            0, nullptr
        );

        av_channel_layout_uninit(&in_ch_layout);
        av_channel_layout_uninit(&out_ch_layout);

        if (ret < 0 || !swr_ctx) {
            throw std::runtime_error("Failed to allocate SwrContext");
        }

        swr_audio_ctx.reset(swr_ctx);

        if (swr_init(swr_audio_ctx.get()) < 0) {
            throw std::runtime_error("Failed to initialize SwrContext");
        }
    }
}

char map_gray_to_ascii(uint8_t gray, std::string_view charset) {
    if (charset.size() < 2) {
        throw std::invalid_argument("Charset must have at least 2 characters");
    }
    size_t index = (static_cast<size_t>(gray) * (charset.size() - 1)) / 255;
    return charset[index];
}

ScaleResult calculate_aspect_ratio(int iw, int ih, int W, int H) {
    if (iw == 0 || ih == 0) {
        return {W, H, 0, 0};
    }
    
    int ch = std::round((double)W * ih / (2.0 * iw));
    int cw = W;

    if (ch > H) {
        ch = H;
        cw = std::round((double)H * 2.0 * iw / ih);
    }

    int pad_x = (W - cw) / 2;
    int pad_y = (H - ch) / 2;
    
    if (pad_x < 0) pad_x = 0;
    if (pad_y < 0) pad_y = 0;
    if (pad_x + cw > W) cw = W - pad_x;
    if (pad_y + ch > H) ch = H - pad_y;

    return {cw, ch, pad_x, pad_y};
}

void encode(const std::string& input_path, const std::string& output_path, int W, int H, std::string_view charset, ColorMode color_mode) {
    if (charset.size() < 2) {
        throw std::invalid_argument("Charset too small");
    }

    FfmpegContext ctx(input_path);
    
    int iw = ctx.codec_ctx->width;
    int ih = ctx.codec_ctx->height;
    ScaleResult scale = calculate_aspect_ratio(iw, ih, W, H);

    ctx.sws_ctx.reset(sws_getContext(
        iw, ih, ctx.codec_ctx->pix_fmt,
        scale.cw, scale.ch, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    ));

    if (!ctx.sws_ctx) {
        throw std::runtime_error("Could not initialize sws context");
    }

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Could not open output file");
    }

    ascv::FileHeader header{};
    std::memcpy(header.magic, ascv::MAGIC, 4);
    header.version = ascv::FORMAT_VERSION;
    header.width = static_cast<uint16_t>(W);
    header.height = static_cast<uint16_t>(H);
    header.frame_count = 0;
    header.fps_numerator = ctx.fps_num;
    header.fps_denominator = ctx.fps_den;
    header.color_mode = static_cast<uint8_t>(color_mode);

    ctx.scaled_frame->format = AV_PIX_FMT_RGB24;
    ctx.scaled_frame->width = scale.cw;
    ctx.scaled_frame->height = scale.ch;
    if (av_frame_get_buffer(ctx.scaled_frame.get(), 32) < 0) {
        throw std::runtime_error("Could not allocate scaled frame buffer");
    }

    size_t S = 1;
    if (color_mode == ascv::ColorMode::MONOCHROME) {
        S = 1;
    } else {
        S = 2; // ANSI_16, ANSI_256, RGB_24 all use 1 byte for color index
    }

    size_t total_cells = static_cast<size_t>(W) * H;

    struct RawFrame {
        std::vector<char> chars;
        std::vector<uint8_t> rgb;
    };
    std::vector<RawFrame> raw_frames;

    auto process_and_write_frame = [&]() {
        sws_scale(ctx.sws_ctx.get(),
                  ctx.frame->data, ctx.frame->linesize, 0, ctx.codec_ctx->height,
                  ctx.scaled_frame->data, ctx.scaled_frame->linesize);

        RawFrame rf;
        rf.chars.assign(total_cells, charset[0]);
        rf.rgb.assign(total_cells * 3, 0);

        for (int y = 0; y < scale.ch; ++y) {
            for (int x = 0; x < scale.cw; ++x) {
                uint8_t* pixel = ctx.scaled_frame->data[0] + y * ctx.scaled_frame->linesize[0] + x * 3;
                uint8_t r = pixel[0];
                uint8_t g = pixel[1];
                uint8_t b = pixel[2];

                uint8_t luminance = static_cast<uint8_t>(std::round(0.299 * r + 0.587 * g + 0.114 * b));
                char ch = map_gray_to_ascii(luminance, charset);

                size_t cell_idx = ((scale.pad_y + y) * W + (scale.pad_x + x));
                rf.chars[cell_idx] = ch;
                rf.rgb[cell_idx * 3 + 0] = r;
                rf.rgb[cell_idx * 3 + 1] = g;
                rf.rgb[cell_idx * 3 + 2] = b;
            }
        }

        raw_frames.push_back(std::move(rf));
    };

    std::vector<uint8_t> audio_data;

    while (av_read_frame(ctx.fmt_ctx.get(), ctx.packet.get()) >= 0) {
        if (ctx.packet->stream_index == ctx.video_stream_index) {
            if (avcodec_send_packet(ctx.codec_ctx.get(), ctx.packet.get()) >= 0) {
                while (avcodec_receive_frame(ctx.codec_ctx.get(), ctx.frame.get()) == 0) {
                    process_and_write_frame();
                }
            }
        } else if (ctx.audio_stream_index >= 0 && ctx.packet->stream_index == ctx.audio_stream_index) {
            if (avcodec_send_packet(ctx.audio_codec_ctx.get(), ctx.packet.get()) >= 0) {
                while (avcodec_receive_frame(ctx.audio_codec_ctx.get(), ctx.audio_frame.get()) == 0) {
                    int64_t delay = swr_get_delay(ctx.swr_audio_ctx.get(), ctx.audio_frame->sample_rate);
                    int out_samples = av_rescale_rnd(
                        delay + ctx.audio_frame->nb_samples,
                        44100,
                        ctx.audio_frame->sample_rate,
                        AV_ROUND_UP
                    );
                    std::vector<uint8_t> resampled_buf(out_samples * 4);
                    uint8_t* out_data[1] = { resampled_buf.data() };
                    int converted_samples = swr_convert(
                        ctx.swr_audio_ctx.get(),
                        out_data,
                        out_samples,
                        (const uint8_t**)ctx.audio_frame->data,
                        ctx.audio_frame->nb_samples
                    );
                    if (converted_samples > 0) {
                        audio_data.insert(audio_data.end(), resampled_buf.data(), resampled_buf.data() + converted_samples * 4);
                    }
                }
            }
        }
        av_packet_unref(ctx.packet.get());
    }

    // Flush decoder
    avcodec_send_packet(ctx.codec_ctx.get(), nullptr);
    while (avcodec_receive_frame(ctx.codec_ctx.get(), ctx.frame.get()) == 0) {
        process_and_write_frame();
    }

    // Flush audio decoder
    if (ctx.audio_stream_index >= 0) {
        avcodec_send_packet(ctx.audio_codec_ctx.get(), nullptr);
        while (avcodec_receive_frame(ctx.audio_codec_ctx.get(), ctx.audio_frame.get()) == 0) {
            int64_t delay = swr_get_delay(ctx.swr_audio_ctx.get(), ctx.audio_frame->sample_rate);
            int out_samples = av_rescale_rnd(
                delay + ctx.audio_frame->nb_samples,
                44100,
                ctx.audio_frame->sample_rate,
                AV_ROUND_UP
            );
            std::vector<uint8_t> resampled_buf(out_samples * 4);
            uint8_t* out_data[1] = { resampled_buf.data() };
            int converted_samples = swr_convert(
                ctx.swr_audio_ctx.get(),
                out_data,
                out_samples,
                (const uint8_t**)ctx.audio_frame->data,
                ctx.audio_frame->nb_samples
            );
            if (converted_samples > 0) {
                audio_data.insert(audio_data.end(), resampled_buf.data(), resampled_buf.data() + converted_samples * 4);
            }
        }
    }

    // Pass 2: Palette Generation
    std::vector<uint8_t> palette_data;
    if (color_mode == ascv::ColorMode::RGB_24) {
        std::unordered_map<uint32_t, uint32_t> color_freq;
        for (const auto& rf : raw_frames) {
            for (size_t i = 0; i < rf.rgb.size(); i += 3) {
                uint32_t c = (rf.rgb[i] << 16) | (rf.rgb[i+1] << 8) | rf.rgb[i+2];
                color_freq[c]++;
            }
        }
        
        struct ColorCount {
            uint32_t color;
            uint32_t count;
        };
        std::vector<ColorCount> sorted_colors;
        sorted_colors.reserve(color_freq.size());
        for (const auto& pair : color_freq) {
            sorted_colors.push_back({pair.first, pair.second});
        }
        
        std::sort(sorted_colors.begin(), sorted_colors.end(), [](const ColorCount& a, const ColorCount& b) {
            return a.count > b.count; // descending
        });
        
        size_t palette_size = std::min<size_t>(256, sorted_colors.size());
        palette_data.reserve(palette_size * 3);
        for (size_t i = 0; i < palette_size; ++i) {
            uint32_t c = sorted_colors[i].color;
            palette_data.push_back(static_cast<uint8_t>((c >> 16) & 0xFF));
            palette_data.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
            palette_data.push_back(static_cast<uint8_t>(c & 0xFF));
        }
    }

    // Pass 3: Quantization & Delta Encoding
    struct BufferedFrame {
        ascv::FrameType type;
        std::vector<uint8_t> rle_data;
    };
    std::vector<BufferedFrame> buffered_frames;

    std::vector<char> frame_buffer(total_cells * S);
    std::vector<char> previous_frame(total_cells * S);

    auto fill_default_cells = [&](std::vector<char>& buf) {
        if (color_mode == ascv::ColorMode::MONOCHROME) {
            std::fill(buf.begin(), buf.end(), charset[0]);
        } else {
            for (size_t i = 0; i < total_cells; ++i) {
                buf[i] = charset[0];
                buf[total_cells + i] = 0;
            }
        }
    };
    fill_default_cells(previous_frame);

    auto write_frame = [&](const std::vector<char>& current_frame, uint32_t f_index) {
        bool is_i_frame = (f_index % 30 == 0);
        std::vector<uint8_t> rle_data;
        ascv::FrameType f_type;
        
        if (is_i_frame) {
            f_type = ascv::FrameType::I_FRAME;
            rle_data = compress_rle(current_frame);
        } else {
            size_t m00_matches = 0;
            for (size_t i = 0; i < total_cells; ++i) {
                bool match = true;
                for (size_t s = 0; s < S; ++s) {
                    if (current_frame[s * total_cells + i] != previous_frame[s * total_cells + i]) {
                        match = false;
                        break;
                    }
                }
                if (match) m00_matches++;
            }
            
            int best_dx = 0;
            int best_dy = 0;
            size_t max_matches = m00_matches;
            
            for (int dy = -3; dy <= 3; ++dy) {
                for (int dx = -5; dx <= 5; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    
                    size_t matches = 0;
                    for (int y = 0; y < H; ++y) {
                        for (int x = 0; x < W; ++x) {
                            int sx = x - dx;
                            int sy = y - dy;
                            if (sx >= 0 && sx < W && sy >= 0 && sy < H) {
                                size_t c_idx = static_cast<size_t>(y) * W + x;
                                size_t p_idx = static_cast<size_t>(sy) * W + sx;
                                bool match = true;
                                for (size_t s = 0; s < S; ++s) {
                                    if (current_frame[s * total_cells + c_idx] != previous_frame[s * total_cells + p_idx]) {
                                        match = false;
                                        break;
                                    }
                                }
                                if (match) matches++;
                            }
                        }
                    }
                    if (matches > max_matches) {
                        max_matches = matches;
                        best_dx = dx;
                        best_dy = dy;
                    }
                }
            }
            
            size_t non_zero_P = total_cells - m00_matches;
            size_t non_zero_M = total_cells - max_matches;
            
            if (static_cast<double>(non_zero_M) < static_cast<double>(non_zero_P) * 0.9) {
                f_type = ascv::FrameType::M_FRAME;
                std::vector<char> predicted_frame = previous_frame;
                for (int y = 0; y < H; ++y) {
                    for (int x = 0; x < W; ++x) {
                        int sx = x - best_dx;
                        int sy = y - best_dy;
                        if (sx >= 0 && sx < W && sy >= 0 && sy < H) {
                            size_t c_idx = static_cast<size_t>(y) * W + x;
                            size_t p_idx = static_cast<size_t>(sy) * W + sx;
                            for (size_t s = 0; s < S; ++s) {
                                predicted_frame[s * total_cells + c_idx] = previous_frame[s * total_cells + p_idx];
                            }
                        }
                    }
                }
                
                std::vector<char> delta_frame(total_cells * S);
                for (size_t i = 0; i < total_cells * S; ++i) {
                    delta_frame[i] = current_frame[i] - predicted_frame[i];
                }
                
                ascv::MoveBlock mb{};
                if (best_dx > 0) { mb.dest_x = best_dx; mb.src_x = 0; mb.width = W - best_dx; }
                else { mb.dest_x = 0; mb.src_x = -best_dx; mb.width = W + best_dx; }
                
                if (best_dy > 0) { mb.dest_y = best_dy; mb.src_y = 0; mb.height = H - best_dy; }
                else { mb.dest_y = 0; mb.src_y = -best_dy; mb.height = H + best_dy; }
                
                std::vector<uint8_t> compressed = compress_rle(delta_frame);
                
                std::vector<uint8_t> payload;
                payload.push_back(1);
                const uint8_t* mb_ptr = reinterpret_cast<const uint8_t*>(&mb);
                payload.insert(payload.end(), mb_ptr, mb_ptr + sizeof(ascv::MoveBlock));
                payload.insert(payload.end(), compressed.begin(), compressed.end());
                
                buffered_frames.push_back({ascv::FrameType::M_FRAME, std::move(payload)});
                previous_frame = current_frame;
                return;
            }

            f_type = ascv::FrameType::P_FRAME;
            std::vector<char> delta_frame(total_cells * S);
            bool all_cells_match = true;
            for (size_t i = 0; i < total_cells * S; ++i) {
                char delta = current_frame[i] - previous_frame[i];
                delta_frame[i] = delta;
                if (delta != 0) {
                    all_cells_match = false;
                }
            }
            if (all_cells_match) {
                f_type = ascv::FrameType::REPEAT_FRAME;
                buffered_frames.push_back({f_type, {}});
                previous_frame = current_frame;
                return;
            }
            rle_data = compress_rle(delta_frame);
        }

        buffered_frames.push_back({f_type, std::move(rle_data)});
        previous_frame = current_frame;
    };

    uint32_t frame_count = 0;
    
    std::unordered_map<uint32_t, uint8_t> color_cache;

    for (const auto& rf : raw_frames) {
        for (size_t i = 0; i < total_cells; ++i) {
            frame_buffer[i] = rf.chars[i];
            if (S == 2) {
                uint8_t r = rf.rgb[i * 3 + 0];
                uint8_t g = rf.rgb[i * 3 + 1];
                uint8_t b = rf.rgb[i * 3 + 2];
                uint32_t c = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
                auto it = color_cache.find(c);
                if (it != color_cache.end()) {
                    frame_buffer[total_cells + i] = it->second;
                } else {
                    uint8_t best_idx = 0;
                    if (color_mode == ascv::ColorMode::ANSI_16) {
                        best_idx = rgb_to_ansi16(r, g, b);
                    } else if (color_mode == ascv::ColorMode::ANSI_256) {
                        best_idx = rgb_to_ansi256(r, g, b);
                    } else if (color_mode == ascv::ColorMode::RGB_24) {
                        int min_dist = 10000000;
                        for (size_t p = 0; p < palette_data.size(); p += 3) {
                            int dr = static_cast<int>(r) - palette_data[p];
                            int dg = static_cast<int>(g) - palette_data[p + 1];
                            int db = static_cast<int>(b) - palette_data[p + 2];
                            int dist = dr*dr + dg*dg + db*db;
                            if (dist < min_dist) {
                                min_dist = dist;
                                best_idx = static_cast<uint8_t>(p / 3);
                            }
                        }
                    }
                    color_cache[c] = best_idx;
                    frame_buffer[total_cells + i] = best_idx;
                }
            }
        }
        write_frame(frame_buffer, frame_count);
        frame_count++;
    }

    // Build training buffers
    std::vector<const std::vector<uint8_t>*> non_empty_frames;
    for (const auto& bf : buffered_frames) {
        if (bf.type != ascv::FrameType::REPEAT_FRAME && !bf.rle_data.empty()) {
            non_empty_frames.push_back(&bf.rle_data);
        }
    }
    
    size_t sample_count = std::min<size_t>(5000, non_empty_frames.size());
    double step = non_empty_frames.size() > 0 ? static_cast<double>(non_empty_frames.size()) / sample_count : 1.0;
    
    std::vector<uint8_t> samplesBuffer;
    std::vector<size_t> samplesSizes;
    for (size_t i = 0; i < sample_count; ++i) {
        size_t idx = static_cast<size_t>(i * step);
        if (idx >= non_empty_frames.size()) idx = non_empty_frames.size() - 1;
        const auto* data = non_empty_frames[idx];
        samplesBuffer.insert(samplesBuffer.end(), data->begin(), data->end());
        samplesSizes.push_back(data->size());
    }
    
    std::vector<uint8_t> dict_buffer(100 * 1024); // 100KB
    size_t dict_size = 0;
    if (!samplesSizes.empty()) {
        dict_size = ZDICT_trainFromBuffer(dict_buffer.data(), dict_buffer.size(),
                                          samplesBuffer.data(), samplesSizes.data(), samplesSizes.size());
        if (ZDICT_isError(dict_size)) {
            throw std::runtime_error("ZDICT training failed: " + std::string(ZDICT_getErrorName(dict_size)));
        }
        dict_buffer.resize(dict_size);
    } else {
        dict_buffer.clear();
    }
    
    header.frame_count = frame_count;
    header.dict_size = static_cast<uint32_t>(dict_buffer.size());
    header.palette_colors = static_cast<uint32_t>(palette_data.size() / 3);
    
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!palette_data.empty()) {
        out.write(reinterpret_cast<const char*>(palette_data.data()), palette_data.size());
    }
    if (!dict_buffer.empty()) {
        out.write(reinterpret_cast<const char*>(dict_buffer.data()), dict_buffer.size());
    }
    
    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    if (!dict_buffer.empty()) {
        ZSTD_CCtx_loadDictionary(cctx, dict_buffer.data(), dict_buffer.size());
    }
    
    for (const auto& bf : buffered_frames) {
        ascv::FrameHeader f_header{};
        f_header.type = bf.type;
        
        if (bf.type == ascv::FrameType::REPEAT_FRAME) {
            f_header.compressed_size = 0;
            out.write(reinterpret_cast<const char*>(&f_header), sizeof(f_header));
        } else {
            size_t max_zstd_size = ZSTD_compressBound(bf.rle_data.size());
            std::vector<uint8_t> compressed_data(max_zstd_size);
            
            size_t c_size = ZSTD_compressCCtx(cctx, compressed_data.data(), compressed_data.size(),
                                              bf.rle_data.data(), bf.rle_data.size(), 3);
            if (ZSTD_isError(c_size)) {
                ZSTD_freeCCtx(cctx);
                throw std::runtime_error("ZSTD compression failed: " + std::string(ZSTD_getErrorName(c_size)));
            }
            
            f_header.compressed_size = static_cast<uint32_t>(c_size);
            out.write(reinterpret_cast<const char*>(&f_header), sizeof(f_header));
            out.write(reinterpret_cast<const char*>(compressed_data.data()), c_size);
        }
    }
    ZSTD_freeCCtx(cctx);

    // Write WAV sidecar if we have audio data
    if (ctx.audio_stream_index >= 0 && !audio_data.empty()) {
        std::ofstream wav_out(output_path + ".wav", std::ios::binary);
        if (wav_out) {
            WavHeader wav_header;
            wav_header.chunk_size = 36 + static_cast<uint32_t>(audio_data.size());
            wav_header.subchunk2_size = static_cast<uint32_t>(audio_data.size());
            wav_out.write(reinterpret_cast<const char*>(&wav_header), sizeof(wav_header));
            wav_out.write(reinterpret_cast<const char*>(audio_data.data()), audio_data.size());
        }
    }
}

} // namespace ascv::encoder
