add_executable(ui main.cpp)

target_link_libraries(ui PRIVATE
    gs2_ui
    gs2_util
    gs2_paths
)
