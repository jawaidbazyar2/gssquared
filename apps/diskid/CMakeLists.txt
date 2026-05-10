add_executable(diskid main.cpp)

target_link_libraries(diskid PRIVATE
    gs2_devices_floppy_woz
    gs2_devices_diskii_fmt
    gs2_util
)
