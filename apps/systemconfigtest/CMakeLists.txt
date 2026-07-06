add_executable(systemconfigtest main.cpp)

target_link_libraries(systemconfigtest PRIVATE
    gs2_systemconfig
)

add_test(NAME systemconfigtest COMMAND systemconfigtest --self-test)
