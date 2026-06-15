#include "ascv/format.hpp"
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "ASCV Encoder v" << ascv::FORMAT_VERSION << std::endl;
    return 0;
}
