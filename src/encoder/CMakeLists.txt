add_executable(ascv_encoder main.cpp)
target_link_libraries(ascv_encoder PRIVATE PkgConfig::FFMPEG libzstd_static)
target_include_directories(ascv_encoder PRIVATE ${PROJECT_SOURCE_DIR}/include)
