#include "ascv/format.hpp"
#include <iostream>
#include <fstream>
#include <string>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int main(int argc, char* argv[]) {
    CLI::App app{"ASCV Encoder"};

    std::string input;
    std::string output;
    int width = 0;
    int height = 0;
    std::string charset = " .:-=+*#%@";

    app.add_option("-i,--input", input, "Input video file")->required();
    app.add_option("-o,--output", output, "Output ASCV file")->required();
    app.add_option("-W,--width", width, "Output ASCII width")->required();
    app.add_option("-H,--height", height, "Output ASCII height")->required();
    app.add_option("--charset", charset, "ASCII character set to use");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    auto logger = spdlog::stdout_color_mt("encoder");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);

    spdlog::info("Starting ASCV Encoder");
    spdlog::info("Input: {}", input);
    spdlog::info("Output: {}", output);
    spdlog::info("Resolution: {}x{}", width, height);

    std::ofstream out(output, std::ios::binary);
    if (!out) {
        spdlog::error("Failed to open output file: {}", output);
        return 1;
    }

    ascv::FileHeader header{};
    std::memcpy(header.magic, ascv::MAGIC, 4);
    header.version = ascv::FORMAT_VERSION;
    header.width = static_cast<uint16_t>(width);
    header.height = static_cast<uint16_t>(height);
    header.frame_count = 0;
    header.fps_numerator = 0;
    header.fps_denominator = 0;

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.close();

    spdlog::info("Successfully wrote ASCV header to {}", output);

    return 0;
}
