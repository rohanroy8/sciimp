#include "ascv/format.hpp"
#include "EncoderCore.hpp"
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

    try {
        ascv::encoder::encode(input, output, width, height, charset);
        spdlog::info("Successfully encoded to {}", output);
    } catch (const std::exception& e) {
        spdlog::error("Encoding failed: {}", e.what());
        return 1;
    }

    return 0;
}
