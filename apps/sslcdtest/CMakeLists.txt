# The LCD check uses the production card and monitor model. CTest supplies the
# distributed original firmware.
add_executable(sslcdtest main.cpp)
target_link_libraries(sslcdtest PRIVATE gs2_devices_secondsight)
target_compile_options(sslcdtest PRIVATE -UNDEBUG)
add_test(NAME sslcdtest COMMAND sslcdtest
    "${PROJECT_SOURCE_DIR}/assets/roms/cards/secondsight/ROM_BEEF.bin")
