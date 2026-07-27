# Build vendored libslirp with GSSquared's glib_compat shim (no system GLib / Meson).
# Include this from the top-level CMakeLists.txt.

set(GS2_LIBSLIRP_ROOT ${CMAKE_SOURCE_DIR}/vendored/libslirp)

set(SLIRP_MAJOR_VERSION 4)
set(SLIRP_MINOR_VERSION 9)
set(SLIRP_MICRO_VERSION 3)
set(SLIRP_VERSION_STRING "\"4.9.3\"")

configure_file(
    ${GS2_LIBSLIRP_ROOT}/src/libslirp-version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/libslirp-generated/libslirp-version.h
    @ONLY
)

set(GS2_LIBSLIRP_SOURCES
    ${GS2_LIBSLIRP_ROOT}/src/arp_table.c
    ${GS2_LIBSLIRP_ROOT}/src/bootp.c
    ${GS2_LIBSLIRP_ROOT}/src/cksum.c
    ${GS2_LIBSLIRP_ROOT}/src/dhcpv6.c
    ${GS2_LIBSLIRP_ROOT}/src/dnssearch.c
    ${GS2_LIBSLIRP_ROOT}/src/if.c
    ${GS2_LIBSLIRP_ROOT}/src/ip6_icmp.c
    ${GS2_LIBSLIRP_ROOT}/src/ip6_input.c
    ${GS2_LIBSLIRP_ROOT}/src/ip6_output.c
    ${GS2_LIBSLIRP_ROOT}/src/ip_icmp.c
    ${GS2_LIBSLIRP_ROOT}/src/ip_input.c
    ${GS2_LIBSLIRP_ROOT}/src/ip_output.c
    ${GS2_LIBSLIRP_ROOT}/src/mbuf.c
    ${GS2_LIBSLIRP_ROOT}/src/misc.c
    ${GS2_LIBSLIRP_ROOT}/src/ncsi.c
    ${GS2_LIBSLIRP_ROOT}/src/ndp_table.c
    ${GS2_LIBSLIRP_ROOT}/src/sbuf.c
    ${GS2_LIBSLIRP_ROOT}/src/slirp.c
    ${GS2_LIBSLIRP_ROOT}/src/socket.c
    ${GS2_LIBSLIRP_ROOT}/src/state.c
    ${GS2_LIBSLIRP_ROOT}/src/stream.c
    ${GS2_LIBSLIRP_ROOT}/src/tcp_input.c
    ${GS2_LIBSLIRP_ROOT}/src/tcp_output.c
    ${GS2_LIBSLIRP_ROOT}/src/tcp_subr.c
    ${GS2_LIBSLIRP_ROOT}/src/tcp_timer.c
    ${GS2_LIBSLIRP_ROOT}/src/tftp.c
    ${GS2_LIBSLIRP_ROOT}/src/udp.c
    ${GS2_LIBSLIRP_ROOT}/src/udp6.c
    ${GS2_LIBSLIRP_ROOT}/src/util.c
    ${GS2_LIBSLIRP_ROOT}/src/version.c
    ${GS2_LIBSLIRP_ROOT}/src/vmstate.c
    ${CMAKE_SOURCE_DIR}/src/devices/uthernet2/glib_compat/glib_compat.cpp
)

add_library(gs2_libslirp STATIC ${GS2_LIBSLIRP_SOURCES})

# glib_compat must come first so #include <glib.h> resolves to our shim.
target_include_directories(gs2_libslirp
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/devices/uthernet2/glib_compat
    PUBLIC
        ${GS2_LIBSLIRP_ROOT}/src
        ${CMAKE_CURRENT_BINARY_DIR}/libslirp-generated
)

target_compile_definitions(gs2_libslirp PRIVATE
    BUILDING_LIBSLIRP
    G_LOG_DOMAIN="Slirp"
    LIBSLIRP_STATIC
)

# Silence noisy upstream warnings; we do not edit vendored sources.
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(gs2_libslirp PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wno-unused-parameter>
        $<$<COMPILE_LANGUAGE:C>:-Wno-sign-compare>
        $<$<COMPILE_LANGUAGE:C>:-Wno-missing-field-initializers>
    )
endif()

if(APPLE)
    target_link_libraries(gs2_libslirp PUBLIC resolv)
elseif(WIN32)
    target_link_libraries(gs2_libslirp PUBLIC ws2_32 iphlpapi)
endif()
