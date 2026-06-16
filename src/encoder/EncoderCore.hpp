#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <cstdint>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
}

namespace ascv::encoder {

struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const {
        avformat_close_input(&ctx);
    }
};

struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const {
        avcodec_free_context(&ctx);
    }
};

struct AVFrameDeleter {
    void operator()(AVFrame* frame) const {
        av_frame_free(&frame);
    }
};

struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const {
        av_packet_free(&pkt);
    }
};

struct SwsContextDeleter {
    void operator()(SwsContext* ctx) const {
        sws_freeContext(ctx);
    }
};

class FfmpegContext {
public:
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> fmt_ctx;
    std::unique_ptr<AVCodecContext, AVCodecContextDeleter> codec_ctx;
    std::unique_ptr<AVFrame, AVFrameDeleter> frame;
    std::unique_ptr<AVFrame, AVFrameDeleter> gray_frame;
    std::unique_ptr<AVPacket, AVPacketDeleter> packet;
    std::unique_ptr<SwsContext, SwsContextDeleter> sws_ctx;

    int video_stream_index = -1;
    double fps = 0.0;
    int fps_num = 0;
    int fps_den = 0;

    FfmpegContext(const std::string& input_path);
    ~FfmpegContext() = default;
};

struct ScaleResult {
    int cw;
    int ch;
    int pad_x;
    int pad_y;
};

ScaleResult calculate_aspect_ratio(int iw, int ih, int W, int H);

void encode(const std::string& input_path, const std::string& output_path, int W, int H, std::string_view charset);
char map_gray_to_ascii(uint8_t gray, std::string_view charset);

} // namespace ascv::encoder
