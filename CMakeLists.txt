cmake_minimum_required(VERSION 3.10)

# Set the project name
project(gssquared)

################################################################################
#### VERSION MANAGEMENT
################################################################################

# Define base version (major.minor)
set(VERSION_MAJOR "0")
set(VERSION_MINOR "7")

# Get git commit count for patch version (auto-incrementing)
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_COUNT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_COMMIT_COUNT)
        set(VERSION_PATCH ${GIT_COMMIT_COUNT})
    else()
        set(VERSION_PATCH "0")
    endif()
    
    # Also get git hash for build info
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    set(VERSION_PATCH "0")
    set(GIT_HASH "unknown")
endif()

# Construct full version string
set(PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

message(STATUS "Project version: ${PROJECT_VERSION} (git: ${GIT_HASH})")

# Generate version header file
configure_file(
    "${PROJECT_SOURCE_DIR}/src/version.h.in"
    "${PROJECT_BINARY_DIR}/generated/version.h"
)

# Generate Info.plist file with version information
configure_file(
    "${PROJECT_SOURCE_DIR}/assets/Info.plist.in"
    "${PROJECT_BINARY_DIR}/generated/Info.plist"
)

# Accepted settings:
# GS2_BUILD_NATIVE     Set to build (on Mac) for only the current (native) architecture. Otherwise, build for both arm64 and x86_64.
# GS2_PROGRAM_FILES    Set to build a directory of program files (bare executable and files instead of platform-specific format).
# GS2_BUNDLE_LIBS      Set to bundle libraries with the build.
# CMAKE_BUILD_TYPE     Release | Debug (default: Release)

if(WIN32) 
    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_SYSTEM_VERSION 10.0)
    set(CMAKE_SYSTEM_PROCESSOR x86_64)
endif()

# Find Clang compilers before project() is called
#if(NOT WIN32)  # For Unix-like systems (Linux and Mac)
    find_program(CLANG_EXECUTABLE NAMES clang)
    find_program(CLANGPP_EXECUTABLE NAMES clang++)

    if(CLANG_EXECUTABLE AND CLANGPP_EXECUTABLE)
        # Only set if not already set in cache
        if(NOT CMAKE_C_COMPILER)
            set(CMAKE_C_COMPILER "${CLANG_EXECUTABLE}" CACHE FILEPATH "C compiler" FORCE)
        endif()
        if(NOT CMAKE_CXX_COMPILER)
            set(CMAKE_CXX_COMPILER "${CLANGPP_EXECUTABLE}" CACHE FILEPATH "C++ compiler" FORCE)
        endif()
    else()
        message(STATUS "Clang not found, using default system compiler")
    endif()
#endif() 

# Define build options and set defaults
option(GS2_BUILD_NATIVE "Build for native architecture only" OFF)
option(GS2_PROGRAM_FILES "Build a directory of program files (bare executable and files instead of bundle/package)" OFF)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND NOT GS2_PROGRAM_FILES)
    option(GS2_BUNDLE_LIBS "Bundle library dependencies with the build" OFF)
else()
    set(GS2_BUNDLE_LIBS ON CACHE BOOL "Bundle library dependencies with the build" FORCE)
endif()

# Set Apple architecture globally if on Apple platform
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
    if(GS2_BUILD_NATIVE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(APPLE_ARCHITECTURE "arm64")
        else()
            set(APPLE_ARCHITECTURE "x86_64")
        endif()
    else()
        set(APPLE_ARCHITECTURE "arm64;x86_64")
    endif()
    set(CMAKE_OSX_ARCHITECTURES ${APPLE_ARCHITECTURE})
    message(STATUS "Building for architectures: ${APPLE_ARCHITECTURE}")
endif()

# Set default install prefix based on build target
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE PATH "Default install path" FORCE)
endif()

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Set default to static for our internal libraries 
set(BUILD_SHARED_LIBS OFF)

# First, create a target to assemble all resources into build/ directory.
add_custom_target(assemble_resources ALL
    # Create resources directory structure
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/resources
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/resources/roms
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/resources/sounds
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/resources/img
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/resources/fonts

    # Copy static resources
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/assets/img/atlas.png ${CMAKE_BINARY_DIR}/resources/img/atlas.png
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/assets/img/gs2.icns ${CMAKE_BINARY_DIR}/resources/gs2.icns
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/roms ${CMAKE_BINARY_DIR}/resources/roms
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/sounds ${CMAKE_BINARY_DIR}/resources/sounds
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/fonts ${CMAKE_BINARY_DIR}/resources/fonts
)

function(configure_sdl_builds)
    set(BUILD_SHARED_LIBS ON)

    # Force SDL to build with the correct SDK
    if(APPLE)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Deployment target" FORCE)
    endif()

    set(SDL_SHARED ON)
    set(SDLTTF_VENDORED ON)

    # Force rebuild SDL with these settings
    add_subdirectory(${CMAKE_SOURCE_DIR}/vendored/SDL)
    add_subdirectory(${CMAKE_SOURCE_DIR}/vendored/SDL_image)
    add_subdirectory(${CMAKE_SOURCE_DIR}/vendored/SDL_ttf)
    set(BUILD_SHARED_LIBS OFF)
endfunction()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

configure_sdl_builds()

# Define flags for different build types
#set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -ggdb3 -fsanitize=address,undefined -fno-omit-frame-pointer")
if(WIN32)
    # Windows debug build without sanitizers
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -ggdb3")
else()
    # Non-Windows debug build with sanitizers
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -ggdb3 -fsanitize=address,undefined -fno-omit-frame-pointer")
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# When using sanitizers, we need to link with them too
#if(CMAKE_BUILD_TYPE STREQUAL "Debug")
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

add_library(gs2_headers INTERFACE)
target_include_directories(gs2_headers
    INTERFACE ${CMAKE_SOURCE_DIR}/src
    INTERFACE ${PROJECT_BINARY_DIR}/generated
    INTERFACE ${CMAKE_BINARY_DIR}/cfg
)

# all following targets will link to SDL3
link_libraries(gs2_headers SDL3::SDL3-shared)

add_library(gs2_devices_game src/devices/game/gamecontroller.cpp src/devices/game/mousewheel.cpp )

add_library(gs2_devices_diskii_fmt
    src/devices/diskii/diskii_fmt.cpp
    src/devices/diskii/diskii_fmt.cpp
)

add_library(gs2_util_audio src/devices/speaker/LowPass.cpp src/devices/speaker/SpeakerX.cpp)

add_library(gs2_devices_pdblock2 src/devices/pdblock2/pdblock2.cpp )

add_library(gs2_devices_prodos_clock src/devices/prodos_clock/prodos_clock.cpp )

add_library(gs2_devices_tcp    src/devices/thunderclock_plus/thunderclockplus.cpp )

add_library(gs2_devices_languagecard     src/devices/languagecard/languagecard.cpp )

add_library(gs2_devices_keyboard     src/devices/keyboard/keyboard.cpp )

add_library(gs2_devices_speaker     src/devices/speaker/speaker.cpp )

add_library(gs2_devices_memexp     src/devices/memoryexpansion/memexp.cpp )

add_library(gs2_devices_parallel     src/devices/parallel/parallel.cpp )

add_library(gs2_devices_videx     src/devices/videx/videx.cpp src/devices/videx/videx_80x24.cpp )

add_library(gs2_devices_mockingboard     src/devices/mockingboard/mb.cpp )

add_library(gs2_devices_annunciator     src/devices/annunciator/annunciator.cpp )

add_library(gs2_devices_mouse     src/devices/applemouseii/mouse.cpp )

add_library(gs2_devices_iiememory src/devices/iiememory/iiememory.cpp)

add_library(gs2_devices_ensoniq src/devices/es5503/ensoniq.cpp src/devices/es5503/soundglu.cpp)

add_library(gs2_devices_scc8530 src/devices/scc8530/scc8530.cpp)

add_library(gs2_devices_iwm src/devices/iwm/iwm_device.cpp)

#add_library(gs2_devices_iigsmemory src/devices/iigsmemory/iigsmemory.cpp)

add_library(gs2_devices_cassette     src/devices/cassette/cassette.cpp )

add_library(gs2_devices_vidhd     src/devices/vidhd/vidhd.cpp )

add_library(gs2_devices_adb     src/devices/adb/keygloo.cpp )

add_library(gs2_trace src/debugger/trace.cpp src/debugger/trace_opcodes.cpp)

add_library(gs2_debugger  src/debugger/debugwindow.cpp src/debugger/MonitorCommand.cpp 
    src/debugger/ExecuteCommand.cpp src/debugger/MemoryWatch.cpp src/debugger/disasm.cpp)

add_library(gs2_mmu src/mmus/mmu.cpp src/mmus/mmu_ii.cpp src/mmus/mmu_iie.cpp src/mmus/mmu_iigs.cpp)

#add_library(gs2_cpu src/cpus/cpu_6502.cpp src/cpus/cpu_65c02.cpp src/cpu.cpp )
#add_library(gs2_cpu src/cpus/core_6502.cpp src/cpu.cpp )
add_library(gs2_cpu src/cpu.cpp )

add_library(gs2_cpu_new src/cpus/cpu_implementations.cpp src/cpus/cpu_6502.cpp src/cpus/cpu_65c02.cpp src/cpus/cpu_65816.cpp)

add_library(gs2_computer src/computer.cpp )

add_library(gs2_event_dispatcher src/util/EventDispatcher.cpp )

add_library(gs2_util src/util/media.cpp src/util/ResourceFile.cpp src/util/dialog.cpp src/util/mount.cpp 
    src/util/soundeffects.cpp src/util/EventQueue.cpp src/util/Event.cpp src/util/EventTimer.cpp src/util/TextRenderer.cpp
    src/util/HexDecode.cpp src/util/DeviceFrameDispatcher.cpp src/util/Metrics.cpp)

add_library(gs2_ui src/ui/AssetAtlas.cpp src/ui/Container.cpp src/ui/DiskII_Button.cpp src/ui/Unidisk_Button.cpp 
    src/ui/MousePositionTile.cpp src/ui/OSD.cpp src/ui/Tile.cpp src/ui/Button.cpp src/ui/MainAtlas.cpp src/ui/ModalContainer.cpp
    src/ui/FadeButton.cpp src/ui/Clipboard.cpp src/ui/TextInput.cpp src/ui/SelectSystem.cpp src/ui/SystemButton.cpp
)

add_library(gs2_displaypp src/devices/displaypp/frame/frame_bit.cpp src/devices/displaypp/frame/frame_byte.cpp src/devices/displaypp/CharRom.cpp src/devices/displaypp/AppleIIgsColors.cpp)

add_library(gs2_ntsc src/display/ntsc.cpp src/display/Matrix3x3.cpp src/display/OEVector.cpp src/display/filters.cpp )

add_library(gs2_message_bus src/mbus/MessageBus.cpp)

add_library(gs2_paths src/paths.cpp)

add_library(gs2_video_scanner 
    src/devices/displaypp/VideoScannerII.cpp 
    src/devices/displaypp/VideoScanGenerator.cpp 
    src/devices/displaypp/VideoScannerIIe.cpp 
    src/devices/displaypp/VideoScannerIIePAL.cpp 
    src/devices/displaypp/VideoScannerIIgs.cpp)

add_library(gs2_devices_rtc_pram src/devices/rtc/rtc_pram.cpp)

target_link_libraries(gs2_ui SDL3_image::SDL3_image-shared SDL3_ttf::SDL3_ttf-shared)
target_link_libraries(gs2_debugger gs2_trace gs2_util SDL3_ttf::SDL3_ttf-shared)
target_link_libraries(gs2_util SDL3_ttf::SDL3_ttf-shared)
target_link_libraries(gs2_computer SDL3_ttf::SDL3_ttf-shared)
target_link_libraries(gs2_cpu gs2_trace)
target_link_libraries(gs2_mmu gs2_trace gs2_cpu)

# Add the executable
add_executable(GSSquared src/gs2.cpp src/debug.cpp  src/opcodes.cpp 
    src/display/text_40x24.cpp src/display/hgr_280x192.cpp src/display/display.cpp
    src/devices/diskii/diskii.cpp
    src/platforms.cpp
    src/devices.cpp src/slots.cpp src/systemconfig.cpp
    src/videosystem.cpp
    src/util/AudioSystem.cpp
    ${CMAKE_SOURCE_DIR}/assets/img/gs2.icns
    )

# Link SDL3 and required frameworks
target_link_libraries(GSSquared PRIVATE 
    SDL3::SDL3-shared
    SDL3_image::SDL3_image-shared
)
if(WIN32)
    # We'll bundle the MinGW runtime DLLs instead of static linking
    # This is more reliable and easier to manage
endif()
target_link_libraries(GSSquared 
    PUBLIC
    gs2_cpu_new
    gs2_computer
    gs2_mmu
    gs2_ntsc
    gs2_devices_tcp 
    gs2_devices_keyboard 
    gs2_devices_diskii_fmt 
    gs2_devices_languagecard 
    gs2_devices_prodos_clock
    gs2_devices_speaker
    gs2_devices_cassette
    gs2_devices_vidhd
    gs2_devices_game
    gs2_devices_memexp
    gs2_devices_parallel
    gs2_devices_videx
    gs2_devices_pdblock2
    gs2_devices_mockingboard
    gs2_devices_annunciator
    gs2_devices_iiememory
    gs2_devices_ensoniq
    gs2_devices_scc8530
    gs2_devices_iwm
    gs2_debugger
    gs2_util
    gs2_util_audio
    gs2_ui
    gs2_event_dispatcher
    gs2_displaypp
    gs2_message_bus
    gs2_paths
    gs2_video_scanner
    gs2_devices_mouse
    gs2_devices_rtc_pram
    gs2_devices_adb
)

add_subdirectory(apps/adbtest)

add_subdirectory(apps/nibblizer)

add_subdirectory(apps/denibblizer)

add_subdirectory(apps/diskid)

#add_subdirectory(apps/speaker) # Temporarily remove until I make a "fake computer" interface.

add_subdirectory(apps/gstrace)

add_subdirectory(apps/iigsmmutest)

add_subdirectory(apps/mmutest)

add_subdirectory(apps/cputest)

add_subdirectory(apps/montest)

add_subdirectory(apps/cycletest)

add_subdirectory(apps/dpp)
add_subdirectory(apps/vpp)

add_subdirectory(apps/iieromcsum)

add_subdirectory(apps/cpu816test)

add_subdirectory(apps/ui)

add_subdirectory(apps/rtctest)

add_subdirectory(apps/doc)

################################################################################
#### Packaging targets per platform
################################################################################

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(GS2_PROGRAM_FILES)
        # PROGRAM FILES: Build bare executable, everything goes to build/ directory
        message(STATUS "Building as PROGRAM FILES: Bare executable and files in one directory")

        # Set RPATH for dev mode to look in build/lib
        set_target_properties(GSSquared PROPERTIES
            OUTPUT_NAME "GSSquared"
            INSTALL_RPATH "@executable_path/lib"
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH_USE_LINK_PATH TRUE
        )

        # Resources are already set up by assemble_resources target
        # SDL libraries are already in build/lib
        # Executable will be in build/GSSquared
        # This gives you the structure: build/GSSquared, build/lib/, build/resources/

    else()
        # BUNDLE: Build app bundle (current behavior)
        message(STATUS "Building as BUNDLE: macOS app bundle")

        # now, we always create a bundle, so we don't need to create a custom target for it
        # Add icon file as a resource for macOS bundles
        set_source_files_properties(${CMAKE_SOURCE_DIR}/assets/img/gs2.icns PROPERTIES RESOURCE TRUE)
        set_target_properties(GSSquared PROPERTIES
            OUTPUT_NAME "GSSquared"
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST ${PROJECT_BINARY_DIR}/generated/Info.plist

            # RPATH for dynamic libraries
            INSTALL_RPATH "@executable_path/../Frameworks"
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH_USE_LINK_PATH TRUE
        )

        # Configure CPack
        set(CPACK_GENERATOR "Bundle")
        set(CPACK_BUNDLE_NAME "GSSquared")
        set(CPACK_BUNDLE_PLIST "${PROJECT_BINARY_DIR}/generated/Info.plist")
        set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/assets/img/gs2.icns")
        # Package version info
        set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
        # Bundle identifier and other metadata
        set(CPACK_BUNDLE_IDENTIFIER "com.bazyar.gs2")
        set(CPACK_PACKAGE_VENDOR "Jawaid Bazyar")

        # Link Mac-specific libraries we need
        target_link_libraries(GSSquared PRIVATE
            "-framework OpenGL" 
            "-framework Cocoa"
        )

        install(TARGETS GSSquared
            BUNDLE DESTINATION .
            COMPONENT Runtime
        )

        # Install SDL targets with all their dependencies
        install(TARGETS 
            SDL3-shared 
            SDL3_image-shared 
            SDL3_ttf-shared
            LIBRARY DESTINATION "GSSquared.app/Contents/Frameworks"
            COMPONENT Runtime
        )
        # Install resources
        install(DIRECTORY "${CMAKE_BINARY_DIR}/resources/"
            DESTINATION "${CPACK_BUNDLE_NAME}.app/Contents/Resources"
            COMPONENT Runtime)

        # Optional: Configure DMG creation
        set(CPACK_DMG_VOLUME_NAME "GSSquared")
        set(CPACK_DMG_FORMAT "UDZO")
        set(CPACK_GENERATOR "Bundle;DragNDrop") # This creates both .app and .dmg

        include(CPack)
    endif()
endif()

## Windows packaging targets

if(WIN32)
    if(GS2_PROGRAM_FILES)
        message(STATUS "Windows: Installing for PROGRAM_FILES (in-place run)")
        # Install main executable
        install(TARGETS GSSquared
            RUNTIME DESTINATION bin
            COMPONENT Runtime
        )
        # Install SDL DLLs to bin
        install(FILES
            ${CMAKE_BINARY_DIR}/SDL3.dll
            ${CMAKE_BINARY_DIR}/SDL3_image.dll
            ${CMAKE_BINARY_DIR}/SDL3_ttf.dll
            DESTINATION bin
            COMPONENT Runtime
        )
        # Install MinGW runtime DLLs to bin
        install(FILES
            C:/msys64/mingw64/bin/libgcc_s_seh-1.dll
            C:/msys64/mingw64/bin/libstdc++-6.dll
            C:/msys64/mingw64/bin/libwinpthread-1.dll
            DESTINATION bin
            COMPONENT Runtime
            OPTIONAL
        )
        # Install import libs to lib (optional for devs)
        install(FILES
            ${CMAKE_BINARY_DIR}/lib/libSDL3.dll.a
            ${CMAKE_BINARY_DIR}/lib/libSDL3_image.dll.a
            ${CMAKE_BINARY_DIR}/lib/libSDL3_ttf.dll.a
            DESTINATION lib
            COMPONENT Runtime
        )
        # Install resources
        install(DIRECTORY ${CMAKE_BINARY_DIR}/resources/
            DESTINATION resources
            COMPONENT Runtime
        )
    else()
        message(STATUS "Windows: Installing for packaging (zip)")
        # Same as above, but set up CPack for zip
        install(TARGETS GSSquared
            RUNTIME DESTINATION .
            COMPONENT Runtime
        )
        install(FILES
            ${CMAKE_BINARY_DIR}/SDL3.dll
            ${CMAKE_BINARY_DIR}/SDL3_image.dll
            ${CMAKE_BINARY_DIR}/SDL3_ttf.dll
            DESTINATION .
            COMPONENT Runtime
        )
        # Install MinGW runtime DLLs to root
        install(FILES
            C:/msys64/mingw64/bin/libgcc_s_seh-1.dll
            C:/msys64/mingw64/bin/libstdc++-6.dll
            C:/msys64/mingw64/bin/libwinpthread-1.dll
            DESTINATION .
            COMPONENT Runtime
            OPTIONAL
        )
        install(FILES
            ${CMAKE_BINARY_DIR}/lib/libSDL3.dll.a
            ${CMAKE_BINARY_DIR}/lib/libSDL3_image.dll.a
            ${CMAKE_BINARY_DIR}/lib/libSDL3_ttf.dll.a
            DESTINATION lib
            COMPONENT Runtime
        )
        install(DIRECTORY ${CMAKE_BINARY_DIR}/resources/
            DESTINATION resources
            COMPONENT Runtime
        )
        # CPack zip config
        set(CPACK_GENERATOR "ZIP")
        set(CPACK_PACKAGE_NAME "gssquared")
        set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
        set(CPACK_PACKAGE_VENDOR "Jawaid Bazyar")
        set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GSSquared Apple II Emulator")
        include(CPack)
    endif()
endif()

# Non-Apple UNIX-specific configuration here
if((UNIX) AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(GS2_PROGRAM_FILES)
        # TODO: Make this work for MSYS; no more hard-coded Linux stuff
        # PROGRAM FILES: Build bare executable, everything goes to build/ directory
        message(STATUS "Building as PROGRAM FILES: Bare executable and files in one directory")

        # Set CPack variables based on platform
        if(WIN32)
            set(CPACK_GENERATOR "ZIP")
        else()
            set(CPACK_GENERATOR "TGZ")
        endif()
        set(CPACK_PACKAGE_NAME "gssquared")
        set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
        set(CPACK_PACKAGE_VENDOR "Jawaid Bazyar")
        set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GSSquared Apple II Emulator")
        
        # Install the main executable
        install(TARGETS GSSquared
            RUNTIME DESTINATION bin
            COMPONENT Runtime
        )

        # Install SDL shared libraries with their symlinks
        install(TARGETS 
            SDL3-shared 
            SDL3_image-shared 
            SDL3_ttf-shared
            LIBRARY DESTINATION lib
            COMPONENT Runtime
        )

        # Install resources (using the already assembled resources from assemble_resources target)
        install(DIRECTORY "${CMAKE_BINARY_DIR}/resources/"
            DESTINATION resources
            COMPONENT Runtime
        )

        # Fix RPATH for the executable
        set_target_properties(GSSquared PROPERTIES
            INSTALL_RPATH "\$ORIGIN/../lib"
            BUILD_WITH_INSTALL_RPATH TRUE
        )

        include(CPack)

    else()
        # GNU INSTALL DIRS: Install files per the GNU installation directories layout
        # Used for both installing into the system (package managers) and AppDir
        message(STATUS "Building as GNU INSTALL DIRS: GNU standard filesystem layout")

        # Set CPack generator based on platform
        if(WIN32)
            set(CPACK_GENERATOR "ZIP")
        else()
            set(CPACK_GENERATOR "TGZ")
        endif()
        set(CPACK_PACKAGE_NAME "gssquared")
        set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
        set(CPACK_PACKAGE_VENDOR "Jawaid Bazyar")
        set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GSSquared Apple II Emulator")

        set(GS2_GNU_INSTALL_DIRS ON)
        include(GNUInstallDirs)
        set(GS2_INSTALL_DATADIR "${CMAKE_INSTALL_DATADIR}/GSSquared")
        install(TARGETS GSSquared
            TYPE RUNTIME
            COMPONENT Runtime
        )
        set_target_properties(GSSquared PROPERTIES
            OUTPUT_NAME "GSSquared"
        )
        if(GS2_BUNDLE_LIBS)
            install(TARGETS 
                SDL3-shared 
                SDL3_image-shared 
                SDL3_ttf-shared
                TYPE LIBRARY
                COMPONENT Runtime
            )
            set_target_properties(GSSquared PROPERTIES
                INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH_USE_LINK_PATH TRUE
            )
        else()
            set_target_properties(GSSquared PROPERTIES
                INSTALL_RPATH "${CMAKE_BINARY_DIR}/lib"
            )
        endif()
        install(DIRECTORY "${CMAKE_BINARY_DIR}/resources/roms"
            DESTINATION "${GS2_INSTALL_DATADIR}"
            COMPONENT Runtime
        )
        install(DIRECTORY "${CMAKE_BINARY_DIR}/resources/sounds"
            DESTINATION "${GS2_INSTALL_DATADIR}"
            COMPONENT Runtime
        )
        install(DIRECTORY "${CMAKE_BINARY_DIR}/resources/fonts"
            DESTINATION "${GS2_INSTALL_DATADIR}"
            COMPONENT Runtime
        )
        install(FILES "${CMAKE_BINARY_DIR}/resources/img/atlas.png"
            DESTINATION "${GS2_INSTALL_DATADIR}/img"
            COMPONENT Runtime
        )
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            # The files here are only used by Linux, not other platforms
            # These are also required for AppDirs, when producing AppImage
            install(FILES assets/img/GSSquared.svg
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
                COMPONENT Runtime
            )
            # Install PNG icons for better file manager support
            install(FILES assets/img/MyIcon.iconset/icon_16x16.png
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/16x16/apps"
                RENAME GSSquared.png
                COMPONENT Runtime
            )
            install(FILES assets/img/MyIcon.iconset/icon_32x32.png
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps"
                RENAME GSSquared.png
                COMPONENT Runtime
            )
            install(FILES assets/img/MyIcon.iconset/icon_128x128.png
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps"
                RENAME GSSquared.png
                COMPONENT Runtime
            )
            install(FILES assets/img/MyIcon.iconset/icon_256x256.png
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
                RENAME GSSquared.png
                COMPONENT Runtime
            )
            install(FILES assets/img/MyIcon.iconset/icon_512x512.png
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps"
                RENAME GSSquared.png
                COMPONENT Runtime
            )
            install(FILES assets/GSSquared.desktop
                DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
                COMPONENT Runtime
            )
        endif()
        include(CPack)
    endif()

    # AppImage build target using linuxdeploy
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Find linuxdeploy
        find_program(LINUXDEPLOY_EXECUTABLE
            NAMES linuxdeploy linuxdeploy-x86_64.AppImage
            PATHS
                ${CMAKE_BINARY_DIR}
                ${CMAKE_SOURCE_DIR}
                /usr/local/bin
                /usr/bin
                $ENV{HOME}/.local/bin
        )

        set(LINUXDEPLOY_APPIMAGE "${CMAKE_BINARY_DIR}/linuxdeploy-x86_64.AppImage")
        set(LINUXDEPLOY_NEEDS_DOWNLOAD FALSE)
        
        if(NOT LINUXDEPLOY_EXECUTABLE)
            # Download linuxdeploy if not found
            set(LINUXDEPLOY_URL "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage")
            set(LINUXDEPLOY_NEEDS_DOWNLOAD TRUE)
            
            message(STATUS "linuxdeploy not found, will download to ${LINUXDEPLOY_APPIMAGE}")
            
            # Find curl or wget for downloading
            find_program(CURL_EXECUTABLE NAMES curl)
            find_program(WGET_EXECUTABLE NAMES wget)
            
            if(CURL_EXECUTABLE)
                add_custom_command(
                    OUTPUT ${LINUXDEPLOY_APPIMAGE}
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}
                    COMMAND ${CURL_EXECUTABLE} -L ${LINUXDEPLOY_URL} -o ${LINUXDEPLOY_APPIMAGE}
                    COMMAND chmod +x ${LINUXDEPLOY_APPIMAGE}
                    COMMENT "Downloading linuxdeploy"
                )
            elseif(WGET_EXECUTABLE)
                add_custom_command(
                    OUTPUT ${LINUXDEPLOY_APPIMAGE}
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}
                    COMMAND ${WGET_EXECUTABLE} ${LINUXDEPLOY_URL} -O ${LINUXDEPLOY_APPIMAGE}
                    COMMAND chmod +x ${LINUXDEPLOY_APPIMAGE}
                    COMMENT "Downloading linuxdeploy"
                )
            else()
                message(FATAL_ERROR "linuxdeploy not found and neither curl nor wget available to download it")
            endif()
            
            set(LINUXDEPLOY_EXECUTABLE ${LINUXDEPLOY_APPIMAGE})
        else()
            message(STATUS "Found linuxdeploy: ${LINUXDEPLOY_EXECUTABLE}")
        endif()

        # Set AppDir path
        set(APPIMAGE_APPDIR "${CMAKE_BINARY_DIR}/AppDir")

        # Prepare dependencies list
        set(APPIMAGE_DEPENDS GSSquared assemble_resources)
        # If we're downloading linuxdeploy, add it as a dependency
        if(LINUXDEPLOY_NEEDS_DOWNLOAD)
            list(APPEND APPIMAGE_DEPENDS ${LINUXDEPLOY_APPIMAGE})
        endif()

        # Create AppImage target
        add_custom_target(appimage
            # Clean and create AppDir
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${APPIMAGE_APPDIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${APPIMAGE_APPDIR}
            # Install into AppDir
            COMMAND ${CMAKE_COMMAND} --install . --prefix ${APPIMAGE_APPDIR}/usr --component Runtime
            # Ensure library directory exists (linuxdeploy will find and bundle libraries automatically)
            COMMAND ${CMAKE_COMMAND} -E make_directory ${APPIMAGE_APPDIR}/usr/${CMAKE_INSTALL_LIBDIR}
            # Copy PNG icon to AppDir root as .DirIcon for file manager display
            # This ensures file managers see a proper PNG icon, not just a symlink
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${APPIMAGE_APPDIR}/usr/share/icons/hicolor/256x256/apps/GSSquared.png
                    ${APPIMAGE_APPDIR}/.DirIcon
            # Run linuxdeploy to create AppImage
            # linuxdeploy will automatically find and bundle dependencies
            # Use PNG icon (256x256) for better file manager support
            # LD_LIBRARY_PATH must include build lib dir and AppDir lib dir so linuxdeploy
            # can resolve vendored SDL3 dependencies (libSDL3_ttf -> libSDL3) when running ldd
            COMMAND ${CMAKE_COMMAND} -E env
                    LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:${APPIMAGE_APPDIR}/usr/${CMAKE_INSTALL_LIBDIR}:$ENV{LD_LIBRARY_PATH}
                    ${LINUXDEPLOY_EXECUTABLE}
                    --appdir ${APPIMAGE_APPDIR}
                    --executable ${APPIMAGE_APPDIR}/usr/bin/GSSquared
                    --desktop-file ${APPIMAGE_APPDIR}/usr/share/applications/GSSquared.desktop
                    --icon-file ${APPIMAGE_APPDIR}/usr/share/icons/hicolor/256x256/apps/GSSquared.png
                    --output appimage
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS ${APPIMAGE_DEPENDS}
            COMMENT "Creating AppImage with linuxdeploy"
        )

        message(STATUS "AppImage target available: cmake --build build --target appimage")
    endif()
endif()

configure_file(src/build_config.hpp.in "${CMAKE_BINARY_DIR}/cfg/build_config.hpp" @ONLY)
