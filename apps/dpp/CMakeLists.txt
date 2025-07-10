add_executable(dpp main.cpp)

add_executable(wood woodeneye-008.c)

target_link_libraries(dpp PRIVATE
    gs2_displaypp
    gs2_ntsc
    gs2_paths
)