add_executable(doc main.cpp)

target_link_libraries(doc PRIVATE
    SDL3::SDL3-shared
    gs2_devices_ensoniq
)