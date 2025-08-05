add_executable(vpp main.cpp)

target_link_libraries(vpp PRIVATE
    gs2_displaypp
    gs2_ntsc
    gs2_paths
    gs2_video_scanner
    gs2_mmu
)
