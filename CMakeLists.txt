# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL

cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(libflowdrop)

option(LIBFLOWDROP_BUILD_STATIC "BUILD STATIC LIBRARIES" ON)
option(LIBFLOWDROP_BUILD_SHARED "BUILD SHARED LIBRARIES" ON)

set(CMAKE_CXX_STANDARD 17)

if (WIN32)
    set(BONJOUR_SDK_HOME "$ENV{BONJOUR_SDK_HOME}")
    if (NOT BONJOUR_SDK_HOME)
        message(FATAL_ERROR "BONJOUR_SDK_HOME is not defined")
    endif ()
    set(DNSSD_INCLUDE_DIR "${BONJOUR_SDK_HOME}/Include")
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DNSSD_LIBRARY "${BONJOUR_SDK_HOME}/Lib/x64/dnssd.lib")
    else ()
        set(DNSSD_LIBRARY "${BONJOUR_SDK_HOME}/Lib/Win32/dnssd.lib")
    endif ()
elseif (APPLE)
    #set(DNSSD_LIBRARY "-framework DNS_SD")
    find_library(DNSSD_LIBRARY dnssd)
    set(DNSSD_INCLUDE_DIR "/usr/include")
else ()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(AVAHI_COMPAT_LIBDNSSD REQUIRED IMPORTED_TARGET avahi-compat-libdns_sd)
    set(DNSSD_INCLUDE_DIR ${AVAHI_COMPAT_LIBDNSSD_INCLUDE_DIRS})
    set(DNSSD_LIBRARY PkgConfig::AVAHI_COMPAT_LIBDNSSD)
endif ()
include_directories(${DNSSD_INCLUDE_DIR})

add_subdirectory(ThirdParty/asio)
add_subdirectory(ThirdParty/bzip2)
add_subdirectory(ThirdParty/curl)
add_subdirectory(ThirdParty/libarchive)
add_subdirectory(ThirdParty/libhv)
add_subdirectory(ThirdParty/nlohmann)
add_subdirectory(ThirdParty/xz)
add_subdirectory(ThirdParty/zlib)
add_subdirectory(ThirdParty/zstd)

set(LIBFLOWDROP_SOURCES
        include/flowdrop.hpp
        src/dns_sd/dns_sd_wrapper.hpp
        src/dns_sd/dns_sd_wrapper_linux.cpp
        src/dns_sd/dns_sd_wrapper_win_mac.cpp
        src/device_info.cpp
        src/discovery.cpp
        src/discovery.hpp
        src/portroller.cpp
        src/portroller.hpp
        src/receiver.cpp
        src/sender.cpp
        src/specification.hpp
        src/static_libs.hpp
        src/pipe.cpp
        src/pipe.hpp
        )

set(library_targets)
if (LIBFLOWDROP_BUILD_SHARED)
    add_library(libflowdrop_shared SHARED ${LIBFLOWDROP_SOURCES})
    list(APPEND library_targets libflowdrop_shared)
endif ()
if (LIBFLOWDROP_BUILD_STATIC)
    add_library(libflowdrop_static STATIC ${LIBFLOWDROP_SOURCES})
    list(APPEND library_targets libflowdrop_static)
endif ()


if (LIBFLOWDROP_BUILD_SHARED)
    target_include_directories(libflowdrop_shared PRIVATE ThirdParty/curl/include)
    target_include_directories(libflowdrop_shared PRIVATE ThirdParty/libhv/include)
    target_include_directories(libflowdrop_shared PRIVATE ThirdParty/nlohmann/include)
endif ()
if (LIBFLOWDROP_BUILD_STATIC)
    target_include_directories(libflowdrop_static PRIVATE ThirdParty/curl/include)
    target_include_directories(libflowdrop_static PRIVATE ThirdParty/libhv/include)
    target_include_directories(libflowdrop_static PRIVATE ThirdParty/nlohmann/include)
endif ()


if (LIBFLOWDROP_BUILD_SHARED)
    target_link_libraries(libflowdrop_shared PRIVATE ${DNSSD_LIBRARY})
    target_link_libraries(libflowdrop_shared PRIVATE asio)
    target_link_libraries(libflowdrop_shared PRIVATE bzip2)
    target_link_libraries(libflowdrop_shared PRIVATE libcurl)
    target_link_libraries(libflowdrop_shared PRIVATE hv_static)
    target_link_libraries(libflowdrop_shared PRIVATE archive_static)
    target_link_libraries(libflowdrop_shared PRIVATE nlohmann_json)
    target_link_libraries(libflowdrop_shared PRIVATE xz)
    target_link_libraries(libflowdrop_shared PRIVATE libzstd_static)
endif ()
if (LIBFLOWDROP_BUILD_STATIC)
    target_link_libraries(libflowdrop_static PRIVATE ${DNSSD_LIBRARY})
    target_link_libraries(libflowdrop_static PRIVATE asio)
    target_link_libraries(libflowdrop_static PRIVATE bzip2)
    target_link_libraries(libflowdrop_static PRIVATE libcurl)
    target_link_libraries(libflowdrop_static PRIVATE hv_static)
    target_link_libraries(libflowdrop_static PRIVATE archive_static)
    target_link_libraries(libflowdrop_static PRIVATE nlohmann_json)
    target_link_libraries(libflowdrop_static PRIVATE xz)
    target_link_libraries(libflowdrop_static PRIVATE libzstd_static)
endif ()

if (WIN32)
    if (LIBFLOWDROP_BUILD_SHARED)
        target_link_libraries(libflowdrop_shared PRIVATE ws2_32)
        target_link_libraries(libflowdrop_shared PRIVATE ntdll)
    endif ()
    if (LIBFLOWDROP_BUILD_STATIC)
        target_link_libraries(libflowdrop_static PRIVATE ws2_32)
        target_link_libraries(libflowdrop_static PRIVATE ntdll)
    endif ()
endif ()
