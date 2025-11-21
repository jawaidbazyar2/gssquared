add_executable(gstrace main.cpp)

target_link_libraries(gstrace PRIVATE
    gs2_debugger
    gs2_trace
    gs2_util
)