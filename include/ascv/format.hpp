// ASCV Binary Format Specification — Little-endian, see D-03
#pragma once

#include <cstdint>
#include <cstring>

namespace ascv {

constexpr uint8_t MAGIC[4] = {'A', 'S', 'C', 'V'};
constexpr uint16_t FORMAT_VERSION = 1;

#pragma pack(push, 1)
struct FileHeader {
    uint8_t  magic[4];
    uint16_t version;
    uint16_t width;
    uint16_t height;
    uint32_t frame_count;
    uint32_t fps_numerator;
    uint32_t fps_denominator;
};
#pragma pack(pop)

inline bool validate_magic(const FileHeader& h) {
    return std::memcmp(h.magic, MAGIC, 4) == 0;
}

} // namespace ascv
