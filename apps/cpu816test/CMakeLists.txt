add_executable(cpu816test main.cpp)

target_link_libraries(cpu816test PRIVATE
    gs2_mmu
    gs2_cpu
    gs2_cpu_new
    gs2_debugger
    gs2_video_scanner
)