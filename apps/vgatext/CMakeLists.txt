add_executable(vgatext main.cpp)

target_link_libraries(vgatext PRIVATE
    SDL3::SDL3-shared
    SDL3_image::SDL3_image-shared
)

add_executable(vgatext2 main2.cpp
    ${CMAKE_SOURCE_DIR}/src/devices/secondsight/vga_render_text_9x16.cpp
)

target_include_directories(vgatext2 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(vgatext2 PRIVATE
    SDL3::SDL3-shared
    SDL3_image::SDL3_image-shared
)

add_executable(vgatext3 main3.cpp)

target_link_libraries(vgatext3 PRIVATE
    SDL3::SDL3-shared
    SDL3_image::SDL3_image-shared
)

add_executable(vgatext4 main4.cpp)

target_link_libraries(vgatext4 PRIVATE
    SDL3::SDL3-shared
    SDL3_image::SDL3_image-shared
)
