cmake_minimum_required(VERSION 3.10)

# Set the project name
project(gssquared)

# Accepted settings:
# GS2_BUILD_NATIVE     Set to build (on Mac) for only the current (native) architecture. Otherwise, build for both arm64 and x86_64.
# GS2_PROGRAM_FILES    Set to build a directory of program files (bare executable and files instead of platform-specific format).
# GS2_BUNDLE_LIBS      Set to bundle libraries with the build.
# CMAKE_BUILD_TYPE     Release | Debug (default: Release)

# Find Clang compilers before project() is called
if(NOT WIN32)  # For Unix-like systems (Linux and Mac)
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
endif()

# Define build options and set defaults
option(GS2_BUILD_NATIVE "Build for native architecture only" OFF)
if(WIN32 AND MSVC)
    set(GS2_PROGRAM_FILES ON CACHE BOOL "Build a directory of program files (bare executable and files instead of bundle/package)" FORCE)
else()
    option(GS2_PROGRAM_FILES "Build a directory of program files (bare executable and files instead of bundle/package)" OFF)
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND NOT GS2_PROGRAM_FILES)
    option(GS2_BUNDLE_LIBS "Bundle library dependencies with the build" OFF)
else()
    set(GS2_BUNDLE_LIBS ON CACHE BOOL "Bundle library dependencies with the build" FORCE)
endif()

# Set Apple architecture globally if on Apple platform
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")
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

# Set default install prefix to the build directory
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
        set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Deployment target" FORCE)
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
#set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

configure_sdl_builds()

# Define flags for different build types
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -ggdb3 -fsanitize=address,undefined -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# When using sanitizers, we need to link with them too
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

add_library(gs2_headers INTERFACE)
target_include_directories(gs2_headers
    INTERFACE ${CMAKE_SOURCE_DIR}/src
    INTERFACE ${CMAKE_BINARY_DIR}/cfg
)

# all following targets will link to SDL3
link_libraries(gs2_headers SDL3::SDL3-shared)

add_library(gs2_devices_game src/devices/game/gamecontroller.cpp src/devices/game/mousewheel.cpp )

add_library(gs2_devices_diskii_fmt
    src/devices/diskii/diskii_fmt.cpp
    src/devices/diskii/diskii_fmt.cpp
)

add_library(gs2_devices_pdblock2 src/devices/pdblock2/pdblock2.cpp )

add_library(gs2_devices_prodos_clock src/devices/prodos_clock/prodos_clock.cpp )

add_library(gs2_devices_tcp    src/devices/thunderclock_plus/thunderclockplus.cpp )

add_library(gs2_devices_languagecard     src/devices/languagecard/languagecard.cpp )

add_library(gs2_devices_keyboard     src/devices/keyboard/keyboard.cpp )

add_library(gs2_devices_speaker     src/devices/speaker/speaker.cpp src/devices/speaker/LowPass.cpp )

add_library(gs2_devices_memexp     src/devices/memoryexpansion/memexp.cpp )

add_library(gs2_devices_parallel     src/devices/parallel/parallel.cpp )

add_library(gs2_devices_videx     src/devices/videx/videx.cpp src/devices/videx/videx_80x24.cpp )

add_library(gs2_devices_mockingboard     src/devices/mockingboard/mb.cpp )

add_library(gs2_devices_annunciator     src/devices/annunciator/annunciator.cpp )

add_library(gs2_debugger src/debugger/trace.cpp src/debugger/trace_opcodes.cpp src/debugger/debugwindow.cpp)

add_library(gs2_cpu src/cpus/cpu_6502.cpp src/cpus/cpu_65c02.cpp )

add_library(gs2_util src/util/media.cpp src/util/ResourceFile.cpp src/util/dialog.cpp src/util/mount.cpp src/util/reset.cpp
    src/util/soundeffects.cpp src/util/EventQueue.cpp src/util/Event.cpp src/util/EventTimer.cpp src/util/TextRenderer.cpp)

add_library(gs2_ui src/ui/AssetAtlas.cpp src/ui/Container.cpp src/ui/DiskII_Button.cpp src/ui/Unidisk_Button.cpp 
    src/ui/MousePositionTile.cpp src/ui/OSD.cpp src/ui/Tile.cpp src/ui/Button.cpp src/ui/MainAtlas.cpp src/ui/ModalContainer.cpp
)
target_link_libraries(gs2_ui SDL3_image::SDL3_image-shared SDL3_ttf::SDL3_ttf-shared)
target_link_libraries(gs2_debugger SDL3_ttf::SDL3_ttf-shared)
target_link_libraries(gs2_util SDL3_ttf::SDL3_ttf-shared)

# Add the executable
add_executable(GSSquared src/gs2.cpp src/bus.cpp src/clock.cpp src/debug.cpp src/cpu.cpp src/memory.cpp src/opcodes.cpp src/test.cpp 
    src/display/text_40x24.cpp src/display/lores_40x48.cpp src/display/hgr_280x192.cpp src/display/display.cpp
    src/display/displayng.cpp src/display/ntsc.cpp src/display/font.cpp src/display/hgr.cpp src/display/Matrix3x3.cpp src/display/lgr.cpp
    src/display/OEVector.cpp src/display/filters.cpp
    src/devices/loader.cpp 
    src/devices/diskii/diskii.cpp
    src/platforms.cpp
    src/event_poll.cpp
    src/devices.cpp src/slots.cpp src/systemconfig.cpp
    src/videosystem.cpp
    src/paths.cpp
    ${CMAKE_SOURCE_DIR}/assets/img/gs2.icns
    )

# Link SDL3 and required frameworks
target_link_libraries(GSSquared PRIVATE 
    SDL3::SDL3-shared
    SDL3_image::SDL3_image-shared
    #SDL3_ttf::SDL3_ttf-shared
)

target_link_libraries(GSSquared 
    PUBLIC gs2_cpu 
    gs2_devices_tcp 
    gs2_devices_keyboard 
    gs2_devices_diskii_fmt 
    gs2_devices_languagecard 
    gs2_devices_prodos_clock
    gs2_devices_speaker
    gs2_devices_game
    gs2_devices_memexp
    gs2_devices_parallel
    gs2_devices_videx
    gs2_devices_pdblock2
    gs2_devices_mockingboard
    gs2_devices_annunciator
    gs2_debugger
    gs2_util
    gs2_ui
)

add_subdirectory(apps/nibblizer)

add_subdirectory(apps/denibblizer)

add_subdirectory(apps/diskid)

add_subdirectory(apps/speaker)

add_subdirectory(apps/gstrace)

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
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/assets/Info.plist

            # RPATH for dynamic libraries
            INSTALL_RPATH "@executable_path/../Frameworks"
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH_USE_LINK_PATH TRUE
        )

        # Configure CPack
        set(CPACK_GENERATOR "Bundle")
        set(CPACK_BUNDLE_NAME "GSSquared")
        set(CPACK_BUNDLE_PLIST "${CMAKE_SOURCE_DIR}/assets/Info.plist")
        set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/assets/img/gs2.icns")
        # Package version info
        set(CPACK_PACKAGE_VERSION "0.3.0")
        set(CPACK_PACKAGE_VERSION_MAJOR "0")
        set(CPACK_PACKAGE_VERSION_MINOR "3")
        set(CPACK_PACKAGE_VERSION_PATCH "0")
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
    add_custom_target(packages ALL
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/packages/gssquared-win64/
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/packages/gssquared-win64/
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:GSSquared> ${CMAKE_BINARY_DIR}/packages/gssquared-win64/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/libSDL3.dll.a ${CMAKE_BINARY_DIR}/packages/gssquared-win64/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/libSDL3_image.dll.a ${CMAKE_BINARY_DIR}/packages/gssquared-win64/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/bin/SDL3.dll    ${CMAKE_BINARY_DIR}/packages/gssquared-win64/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/bin/SDL3_image.dll    ${CMAKE_BINARY_DIR}/packages/gssquared-win64/
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/packages/gssquared-win64/resources/roms
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/roms ${CMAKE_BINARY_DIR}/packages/gssquared-win64/resources/roms
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/packages/gssquared-win64/resources/sounds
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/assets/sounds ${CMAKE_BINARY_DIR}/packages/gssquared-win64/resources/sounds
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/packages/gssquared-win64/resources/img
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/assets/img/atlas.png ${CMAKE_BINARY_DIR}/packages/gssquared-win64/resources/img/atlas.png
        DEPENDS GSSquared
    )
endif()

# Non-Apple UNIX-specific configuration here
if((UNIX OR MINGW) AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(GS2_PROGRAM_FILES AND LINUX)
        # TODO: Make this work for MSYS; no more hard-coded Linux stuff
        # PROGRAM FILES: Build bare executable, everything goes to build/ directory
        message(STATUS "Building as PROGRAM FILES: Bare executable and files in one directory")

        # Set RPATH for program files build to look in linux-cli/lib
        set_target_properties(GSSquared PROPERTIES
            OUTPUT_NAME "GSSquared"
            INSTALL_RPATH "$ORIGIN/lib"
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH_USE_LINK_PATH TRUE
        )

        # Resources are already set up by assemble_resources target
        # SDL libraries are already in build/lib
        # Executable will be in build/GSSquared
        # This gives you the structure: build/GSSquared, build/lib/, build/resources/

        # Find the SDL libraries regardless of their exact filenames
        file(GLOB SDL3_LIBS "${CMAKE_BINARY_DIR}/lib/libSDL3.so*")
        file(GLOB SDL3_IMAGE_LIBS "${CMAKE_BINARY_DIR}/lib/libSDL3_image.so*")

        # Custom target for creating Linux package
        add_custom_target(packages
            # Delete contents of any current resources directory
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/resources

            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/packages/linux-cli/resources/roms
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/roms ${CMAKE_BINARY_DIR}/packages/linux-cli/resources/roms
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/packages/linux-cli/resources/sounds
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/sounds ${CMAKE_BINARY_DIR}/packages/linux-cli/resources/sounds
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/packages/linux-cli/resources/fonts
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/fonts ${CMAKE_BINARY_DIR}/packages/linux-cli/resources/fonts
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:GSSquared> ${CMAKE_BINARY_DIR}/packages/linux-cli/
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/packages/linux-cli/resources/img
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/assets/img/atlas.png ${CMAKE_BINARY_DIR}/packages/linux-cli/resources/img/atlas.png
            # Copy SDL dynamic libraries with correct names
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/packages/linux-cli/lib
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/lib/libSDL3.so.0 ${CMAKE_BINARY_DIR}/packages/linux-cli/lib/
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/lib/libSDL3_image.so.0 ${CMAKE_BINARY_DIR}/packages/linux-cli/lib/
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/lib/libSDL3_ttf.so.0 ${CMAKE_BINARY_DIR}/packages/linux-cli/lib/
            # Create necessary symlinks
            COMMAND ${CMAKE_COMMAND} -E create_symlink libSDL3.so.0 ${CMAKE_BINARY_DIR}/packages/linux-cli/lib/libSDL3.so
            COMMAND ${CMAKE_COMMAND} -E create_symlink libSDL3_image.so.0 ${CMAKE_BINARY_DIR}/packages/linux-cli/lib/libSDL3_image.so
            COMMAND ${CMAKE_COMMAND} -E create_symlink libSDL3_ttf.so.0 ${CMAKE_BINARY_DIR}/packages/linux-cli/lib/libSDL3_ttf.so
            # Fix up runtime paths with chrpath
            COMMAND chrpath -r '$$ORIGIN/lib' ${CMAKE_BINARY_DIR}/packages/linux-cli/GSSquared
            COMMAND chrpath -r '$$ORIGIN' ${CMAKE_BINARY_DIR}/packages/linux-cli/lib/libSDL3_image.so.0
            COMMAND chrpath -r '$$ORIGIN' ${CMAKE_BINARY_DIR}/packages/linux-cli/lib/libSDL3_ttf.so.0
            DEPENDS GSSquared
        )
    else()
        # GNU INSTALL DIRS: Install files per the GNU installation directories layout
        # Used for both installing into the system (package managers) and AppDir
        message(STATUS "Building as GNU INSTALL DIRS: GNU standard filesystem layout")

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
        install(DIRECTORY roms
            DESTINATION "${GS2_INSTALL_DATADIR}"
            COMPONENT Runtime
        )
        install(DIRECTORY assets/sounds
            DESTINATION "${GS2_INSTALL_DATADIR}"
            COMPONENT Runtime
        )
        install(DIRECTORY assets/fonts
            DESTINATION "${GS2_INSTALL_DATADIR}"
            COMPONENT Runtime
        )
        install(FILES assets/img/atlas.png
            DESTINATION "${GS2_INSTALL_DATADIR}/img"
            COMPONENT Runtime
        )
        if(LINUX)
            # The files here are only used by Linux, not other platforms
            # These are also required for AppDirs, when producing AppImage
            install(FILES assets/img/GSSquared.svg
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
                COMPONENT Runtime
            )
            install(FILES assets/GSSquared.desktop
                DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
                COMPONENT Runtime
            )
        endif()
    endif()
endif()

configure_file(src/build_config.hpp.in "${CMAKE_BINARY_DIR}/cfg/build_config.hpp" @ONLY)
