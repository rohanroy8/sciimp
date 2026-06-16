#include <catch2/catch_test_macros.hpp>
#include "../src/encoder/EncoderCore.hpp"

using namespace ascv::encoder;

TEST_CASE("1920x1080 into 80x24 produces letterbox", "[aspect_ratio]") {
    // Expected: round(80*1080/(2*1920)) = 22.5 -> 23
    ScaleResult result = calculate_aspect_ratio(1920, 1080, 80, 24);
    REQUIRE(result.cw == 80);
    REQUIRE(result.ch == 23);
    REQUIRE(result.pad_x == 0);
    REQUIRE(result.pad_y == 0); // (24 - 23)/2 = 0
}

TEST_CASE("1920x1080 into 120x60 produces letterbox", "[aspect_ratio]") {
    // Expected: round(120*1080/(2*1920)) = 33.75 -> 34
    ScaleResult result = calculate_aspect_ratio(1920, 1080, 120, 60);
    REQUIRE(result.cw == 120);
    REQUIRE(result.ch == 34);
    REQUIRE(result.pad_x == 0);
    REQUIRE(result.pad_y == 13); // (60 - 34)/2 = 13
}

TEST_CASE("480x640 portrait into 80x24 produces pillarbox", "[aspect_ratio]") {
    // Initially ch = round(80*640/(2*480)) = 53
    // Since 53 > 24, we cap ch = 24.
    // Then cw = round(24*2*480/640) = 36
    ScaleResult result = calculate_aspect_ratio(480, 640, 80, 24);
    REQUIRE(result.ch == 24);
    REQUIRE(result.cw == 36);
    REQUIRE(result.pad_x == 22); // (80 - 36)/2 = 22
    REQUIRE(result.pad_y == 0);
}

TEST_CASE("Square 100x100 into 80x24", "[aspect_ratio]") {
    // Initially ch = round(80*100/(2*100)) = 40
    // Since 40 > 24, we cap ch = 24.
    // Then cw = round(24*2*100/100) = 48
    ScaleResult result = calculate_aspect_ratio(100, 100, 80, 24);
    REQUIRE(result.ch == 24);
    REQUIRE(result.cw == 48);
    REQUIRE(result.pad_x == 16); // (80 - 48)/2 = 16
    REQUIRE(result.pad_y == 0);
}
