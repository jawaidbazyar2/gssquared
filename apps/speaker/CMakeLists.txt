add_executable(speaker main.cpp)

add_executable(newspeakfp newspeakfp.cpp)

add_executable(newspeakfx newspeakfx.cpp)

# Link SDL3 and required frameworks
target_link_libraries(speaker PRIVATE 
    SDL3::SDL3-shared
    SDL3_image::SDL3_image-shared
)
target_link_libraries(newspeakfp PRIVATE 
    SDL3::SDL3-shared
    SDL3_image::SDL3_image-shared
)
target_link_libraries(newspeakfx PRIVATE 
    SDL3::SDL3-shared
    SDL3_image::SDL3_image-shared
)
target_link_libraries(speaker PRIVATE
    gs2_util
    gs2_mmu
    gs2_cpu
    gs2_cpu_new
    gs2_util_audio
    gs2_trace
)
target_link_libraries(newspeakfp PRIVATE
    gs2_util
    gs2_mmu
    gs2_cpu
    gs2_cpu_new
    gs2_util_audio
    gs2_trace
)
target_link_libraries(newspeakfx PRIVATE
    gs2_util
    gs2_mmu
    gs2_cpu
    gs2_cpu_new
    gs2_util_audio
    gs2_trace
)
