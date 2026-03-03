add_executable(mousebugtest main.cpp)

target_link_libraries(mousebugtest PRIVATE
    SDL3::SDL3-shared
)
