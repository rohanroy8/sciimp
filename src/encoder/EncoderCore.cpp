#include "EncoderCore.hpp"
#include "ascv/format.hpp"
#include <stdexcept>
#include <fstream>
#include <vector>

namespace ascv::encoder {

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

void encode(const std::string& input_path, const std::string& output_path, int W, int H, std::string_view charset) {
    if (charset.size() < 2) {
        throw std::invalid_argument("Charset too small");
    }

    FfmpegContext ctx(input_path);

    ctx.sws_ctx.reset(sws_getContext(
        ctx.codec_ctx->width, ctx.codec_ctx->height, ctx.codec_ctx->pix_fmt,
        W, H, AV_PIX_FMT_GRAY8,
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
    ctx.gray_frame->width = W;
    ctx.gray_frame->height = H;
    if (av_frame_get_buffer(ctx.gray_frame.get(), 32) < 0) {
        throw std::runtime_error("Could not allocate gray frame buffer");
    }

    uint32_t frame_count = 0;
    std::vector<char> frame_buffer(W * H);

    while (av_read_frame(ctx.fmt_ctx.get(), ctx.packet.get()) >= 0) {
        if (ctx.packet->stream_index == ctx.video_stream_index) {
            if (avcodec_send_packet(ctx.codec_ctx.get(), ctx.packet.get()) >= 0) {
                while (avcodec_receive_frame(ctx.codec_ctx.get(), ctx.frame.get()) == 0) {
                    sws_scale(ctx.sws_ctx.get(),
                              ctx.frame->data, ctx.frame->linesize, 0, ctx.codec_ctx->height,
                              ctx.gray_frame->data, ctx.gray_frame->linesize);

                    for (int y = 0; y < H; ++y) {
                        for (int x = 0; x < W; ++x) {
                            uint8_t gray = ctx.gray_frame->data[0][y * ctx.gray_frame->linesize[0] + x];
                            frame_buffer[y * W + x] = map_gray_to_ascii(gray, charset);
                        }
                    }

                    out.write(frame_buffer.data(), frame_buffer.size());
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

        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                uint8_t gray = ctx.gray_frame->data[0][y * ctx.gray_frame->linesize[0] + x];
                frame_buffer[y * W + x] = map_gray_to_ascii(gray, charset);
            }
        }

        out.write(frame_buffer.data(), frame_buffer.size());
        frame_count++;
    }

    header.frame_count = frame_count;
    out.seekp(0);
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
}

} // namespace ascv::encoder
