add_executable(ssgputexttest
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/devices/secondsight/ss_gpu_text.cpp
)

add_test(NAME ssgputexttest COMMAND ssgputexttest)
