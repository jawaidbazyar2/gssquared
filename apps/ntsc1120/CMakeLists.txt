add_executable(ntsc1120 main.cpp)

target_include_directories(ntsc1120 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ntsc1120 PRIVATE
    gs2_ntsc
    ${GS2_SDL3}
    ${GS2_SDL3_IMAGE}
)

add_executable(ntscview ntscview.cpp)

target_include_directories(ntscview PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ntscview PRIVATE
    gs2_ntsc
    ${GS2_SDL3}
    ${GS2_SDL3_IMAGE}
)
