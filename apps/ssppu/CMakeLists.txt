add_executable(ssppu main.cpp
    ${CMAKE_SOURCE_DIR}/src/devices/secondsight/ppu_render.cpp
)

target_include_directories(ssppu PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ssppu PRIVATE
    SDL3::SDL3-shared
)

add_executable(chr2tiles chr2tiles.cpp)
