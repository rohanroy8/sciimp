add_executable(ascv_encoder main.cpp)
target_link_libraries(ascv_encoder PRIVATE PkgConfig::FFMPEG libzstd_static CLI11::CLI11 spdlog::spdlog)
target_include_directories(ascv_encoder PRIVATE ${PROJECT_SOURCE_DIR}/include)
