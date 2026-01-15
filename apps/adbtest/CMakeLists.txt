if(APPLE)
    add_executable(adbtest main.cpp menu.mm)

    target_link_libraries(adbtest PRIVATE
        SDL3::SDL3-shared
        gs2_headers
        "-framework Cocoa"
    )
endif()
