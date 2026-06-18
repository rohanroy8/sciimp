// ASCV Binary Format Specification — Little-endian, see D-03
#pragma once

#include <cstdint>
#include <cstring>

namespace ascv {

constexpr uint8_t MAGIC[4] = {'A', 'S', 'C', 'V'};
constexpr uint16_t FORMAT_VERSION = 4;

enum class ColorMode : uint8_t {
    MONOCHROME = 0,
    ANSI_16 = 1,
    ANSI_256 = 2,
    RGB_24 = 3
};

#pragma pack(push, 1)
struct FileHeader {
    uint8_t  magic[4];          // 'A', 'S', 'C', 'V'
    uint16_t version;           // FORMAT_VERSION
    uint16_t width;
    uint16_t height;
    uint32_t frame_count;
    uint32_t fps_numerator;
    uint32_t fps_denominator;
    uint8_t  color_mode; // ascv::ColorMode mapped to uint8_t
    uint32_t palette_colors;    // Number of colors in the embedded palette
    uint32_t dict_size;
};

enum class FrameType : uint8_t {
    I_FRAME = 0,
    P_FRAME = 1,
    REPEAT_FRAME = 2
};

struct FrameHeader {
    FrameType type;
    uint32_t compressed_size;
};
#pragma pack(pop)

inline bool validate_magic(const FileHeader& h) {
    return std::memcmp(h.magic, MAGIC, 4) == 0;
}

} // namespace ascv
