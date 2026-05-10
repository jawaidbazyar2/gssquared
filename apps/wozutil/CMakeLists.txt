add_executable(wozutil main.cpp)

target_link_libraries(wozutil PRIVATE
    gs2_devices_floppy_woz
    gs2_devices_diskii_fmt
    gs2_util
)
