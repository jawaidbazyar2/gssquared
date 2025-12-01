add_executable(iigsmmutest main.cpp)

target_link_libraries(iigsmmutest PRIVATE
    gs2_paths
)