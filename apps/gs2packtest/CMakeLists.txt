add_executable(gs2packtest main.cpp)

target_link_libraries(gs2packtest PRIVATE
    gs2_systemconfig
)

add_test(NAME gs2packtest COMMAND gs2packtest)
