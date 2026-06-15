add_executable(ascv_player main.cpp)
target_link_libraries(ascv_player PRIVATE miniaudio libzstd_static)
target_include_directories(ascv_player PRIVATE ${PROJECT_SOURCE_DIR}/include)
