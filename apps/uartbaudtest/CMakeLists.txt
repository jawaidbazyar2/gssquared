add_executable(uartbaudtest main.cpp)

target_link_libraries(uartbaudtest PRIVATE
    gs2_serial_devices
    gs2_util
)

add_test(NAME uartbaudtest COMMAND uartbaudtest)
