#include "ascv/format.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "ASCV Player v" << ascv::FORMAT_VERSION << std::endl;
    return 0;
}
