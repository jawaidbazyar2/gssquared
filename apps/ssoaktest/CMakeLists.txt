# Document-derived Oak/IBM VGA regression checks use the card test library.
add_executable(ssoaktest main.cpp)
target_link_libraries(ssoaktest PRIVATE gs2_devices_secondsight_test)
target_compile_options(ssoaktest PRIVATE -UNDEBUG)
add_test(NAME ssoaktest COMMAND ssoaktest)
