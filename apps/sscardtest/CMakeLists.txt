# Build the Second Sight reset/lifetime contract check from main.cpp and the production
# card library. CTest invokes the executable; assertions determine success.
add_executable(sscardtest main.cpp)

target_link_libraries(sscardtest PRIVATE
    gs2_devices_secondsight
)

# Assertions are the test result, including in Release builds.
target_compile_options(sscardtest PRIVATE -UNDEBUG)

add_test(NAME sscardtest COMMAND sscardtest)
