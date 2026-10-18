add_executable(vsgtest main.cpp)

target_link_libraries(vsgtest PRIVATE
    gs2_displaypp
    gs2_ntsc
    gs2_paths
    gs2_video_scanner
    gs2_mmu
)

add_test(NAME vsgtest COMMAND vsgtest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
