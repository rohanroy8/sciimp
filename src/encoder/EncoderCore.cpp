#include "EncoderCore.hpp"
#include "ascv/format.hpp"
#include <zstd.h>
#include <stdexcept>
#include <fstream>
#include <cmath>
#include <vector>

namespace ascv::encoder {

namespace {

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
    gray_frame.reset(av_frame_alloc());
    packet.reset(av_packet_alloc());

    if (!frame || !gray_frame || !packet) {
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

void encode(const std::string& input_path, const std::string& output_path, int W, int H, std::string_view charset) {
    if (charset.size() < 2) {
        throw std::invalid_argument("Charset too small");
    }

    FfmpegContext ctx(input_path);
    
    int iw = ctx.codec_ctx->width;
    int ih = ctx.codec_ctx->height;
    ScaleResult scale = calculate_aspect_ratio(iw, ih, W, H);

    ctx.sws_ctx.reset(sws_getContext(
        iw, ih, ctx.codec_ctx->pix_fmt,
        scale.cw, scale.ch, AV_PIX_FMT_GRAY8,
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

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    ctx.gray_frame->format = AV_PIX_FMT_GRAY8;
    ctx.gray_frame->width = scale.cw;
    ctx.gray_frame->height = scale.ch;
    if (av_frame_get_buffer(ctx.gray_frame.get(), 32) < 0) {
        throw std::runtime_error("Could not allocate gray frame buffer");
    }

    uint32_t frame_count = 0;
    std::vector<char> frame_buffer(W * H, charset[0]);
    std::vector<char> previous_frame(W * H, charset[0]);

    auto write_frame = [&](const std::vector<char>& current_frame, uint32_t f_index) {
        bool is_i_frame = (f_index % 30 == 0);
        std::vector<uint8_t> rle_data;
        ascv::FrameHeader f_header{};
        
        if (is_i_frame) {
            f_header.type = ascv::FrameType::I_FRAME;
            rle_data = compress_rle(current_frame);
        } else {
            f_header.type = ascv::FrameType::P_FRAME;
            std::vector<char> delta_frame(W * H);
            for (size_t i = 0; i < current_frame.size(); ++i) {
                if (current_frame[i] == previous_frame[i]) {
                    delta_frame[i] = '\0';
                } else {
                    delta_frame[i] = current_frame[i];
                }
            }
            rle_data = compress_rle(delta_frame);
        }

        size_t max_zstd_size = ZSTD_compressBound(rle_data.size());
        std::vector<uint8_t> compressed_data(max_zstd_size);
        size_t c_size = ZSTD_compress(compressed_data.data(), compressed_data.size(),
                                      rle_data.data(), rle_data.size(), 3);
        if (ZSTD_isError(c_size)) {
            throw std::runtime_error("ZSTD compression failed: " + std::string(ZSTD_getErrorName(c_size)));
        }
        compressed_data.resize(c_size);

        f_header.compressed_size = static_cast<uint32_t>(c_size);
        out.write(reinterpret_cast<const char*>(&f_header), sizeof(f_header));
        out.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_data.size());

        previous_frame = current_frame;
    };

    while (av_read_frame(ctx.fmt_ctx.get(), ctx.packet.get()) >= 0) {
        if (ctx.packet->stream_index == ctx.video_stream_index) {
            if (avcodec_send_packet(ctx.codec_ctx.get(), ctx.packet.get()) >= 0) {
                while (avcodec_receive_frame(ctx.codec_ctx.get(), ctx.frame.get()) == 0) {
                    sws_scale(ctx.sws_ctx.get(),
                              ctx.frame->data, ctx.frame->linesize, 0, ctx.codec_ctx->height,
                              ctx.gray_frame->data, ctx.gray_frame->linesize);

                    std::fill(frame_buffer.begin(), frame_buffer.end(), charset[0]);

                    for (int y = 0; y < scale.ch; ++y) {
                        for (int x = 0; x < scale.cw; ++x) {
                            uint8_t gray = ctx.gray_frame->data[0][y * ctx.gray_frame->linesize[0] + x];
                            frame_buffer[(scale.pad_y + y) * W + (scale.pad_x + x)] = map_gray_to_ascii(gray, charset);
                        }
                    }

                    write_frame(frame_buffer, frame_count);
                    frame_count++;
                }
            }
        }
        av_packet_unref(ctx.packet.get());
    }

    // Flush decoder
    avcodec_send_packet(ctx.codec_ctx.get(), nullptr);
    while (avcodec_receive_frame(ctx.codec_ctx.get(), ctx.frame.get()) == 0) {
        sws_scale(ctx.sws_ctx.get(),
                  ctx.frame->data, ctx.frame->linesize, 0, ctx.codec_ctx->height,
                  ctx.gray_frame->data, ctx.gray_frame->linesize);

        std::fill(frame_buffer.begin(), frame_buffer.end(), charset[0]);

        for (int y = 0; y < scale.ch; ++y) {
            for (int x = 0; x < scale.cw; ++x) {
                uint8_t gray = ctx.gray_frame->data[0][y * ctx.gray_frame->linesize[0] + x];
                frame_buffer[(scale.pad_y + y) * W + (scale.pad_x + x)] = map_gray_to_ascii(gray, charset);
            }
        }

        write_frame(frame_buffer, frame_count);
        frame_count++;
    }

    header.frame_count = frame_count;
    out.seekp(0);
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
}

} // namespace ascv::encoder
