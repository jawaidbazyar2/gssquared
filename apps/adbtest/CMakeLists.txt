if(APPLE)
    #add_executable(adbtest main.cpp menu.mm)
    add_executable(adbtest main.cpp menu.mm ${CMAKE_SOURCE_DIR}/src/debug.cpp)


    target_link_libraries(adbtest PRIVATE
        gs2_util
        SDL3::SDL3-shared
        gs2_headers
        "-framework Cocoa"
    )
endif()
