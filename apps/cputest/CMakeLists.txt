add_executable(cputest main.cpp)

target_link_libraries(cputest PRIVATE
    gs2_mmu
    gs2_cpu
    gs2_cpu_new
    gs2_debugger
    gs2_video_scanner
)