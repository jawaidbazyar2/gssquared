# The geometry check uses the production card and the same rectangle helper as
# both monitor views. CTest supplies the distributed original firmware.
add_executable(ssgeometrytest main.cpp)
target_link_libraries(ssgeometrytest PRIVATE gs2_devices_secondsight)
target_compile_options(ssgeometrytest PRIVATE -UNDEBUG)
add_test(NAME ssgeometrytest COMMAND ssgeometrytest
    "${PROJECT_SOURCE_DIR}/assets/roms/cards/secondsight/ROM_BEEF.bin")
