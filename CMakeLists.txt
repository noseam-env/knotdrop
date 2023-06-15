# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL

cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

cmake_policy(SET CMP0077 NEW)

project(libflowdrop)

option(LIBFLOWDROP_BUILD_STATIC "BUILD STATIC LIBRARIES" ON)
option(LIBFLOWDROP_BUILD_SHARED "BUILD SHARED LIBRARIES" ON)
option(USE_APPLE_DNS_SD "USE APPLE DNS_SD" ON)

set(CMAKE_CXX_STANDARD 17)

set(LIBFLOWDROP_SOURCES
        src/dns_sd/dns_sd_wrapper.hpp
        src/dns_sd/dns_sd_wrapper_avahi.cpp
        src/dns_sd/dns_sd_wrapper_apple.cpp
        src/device_info.cpp
        src/discovery.cpp
        src/discovery.hpp
        src/portroller.cpp
        src/portroller.hpp
        src/receiver.cpp
        src/sender.cpp
        src/specification.hpp
        )

set(LIBFLOWDROP_LIBS
        libcurl
        hv_static
        virtualtfa_static
        nlohmann_json::nlohmann_json)

set(LIBFLOWDROP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(LIBFLOWDROP_TARGET_INCLUDE ${LIBHV_HEADERS})

set(VIRTUALTFA_BUILD_SHARED OFF)

add_subdirectory(ThirdParty/curl)
add_subdirectory(ThirdParty/libhv)
add_subdirectory(ThirdParty/libvirtualtfa)
add_subdirectory(ThirdParty/nlohmann_json)

if (USE_APPLE_DNS_SD)
    if (WIN32)
        set(BONJOUR_SDK_HOME "$ENV{BONJOUR_SDK_HOME}")
        if (NOT BONJOUR_SDK_HOME)
            message(FATAL_ERROR "BONJOUR_SDK_HOME is not defined")
        endif ()
        set(DNSSD_INCLUDE_DIR "${BONJOUR_SDK_HOME}/Include")
        # TODO: ARM support
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
    add_definitions(-DUSE_APPLE_DNS_SD)
endif ()

if (DNSSD_INCLUDE_DIR)
    set(LIBFLOWDROP_TARGET_INCLUDE ${LIBFLOWDROP_TARGET_INCLUDE} ${DNSSD_INCLUDE_DIR})
    #list(APPEND LIBFLOWDROP_TARGET_INCLUDE ${DNSSD_INCLUDE_DIR})
endif ()
if (DNSSD_LIBRARY)
    list(APPEND LIBFLOWDROP_LIBS ${DNSSD_LIBRARY})
endif ()

if (WIN32)
    option(BUILD_FOR_MT "build for /MT" OFF)
    if(BUILD_FOR_MT)
        # MultiThreaded
        # Compile with -MT or equivalent flag(s) to use a multi-threaded statically-linked runtime library.
        #
        # MultiThreadedDLL
        # Compile with -MD or equivalent flag(s) to use a multi-threaded dynamically-linked runtime library.
        #
        # MultiThreadedDebug
        # Compile with -MTd or equivalent flag(s) to use a multi-threaded statically-linked runtime library.
        #
        # MultiThreadedDebugDLL
        # Compile with -MDd or equivalent flag(s) to use a multi-threaded dynamically-linked runtime library.

        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    endif()

    list(APPEND LIBFLOWDROP_LIBS ws2_32 ntdll)
endif ()

if (UNIX)
    set_property(TARGET libcurl PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET hv_static PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()

if (LIBFLOWDROP_BUILD_SHARED)
    add_library(libflowdrop_shared SHARED ${LIBFLOWDROP_SOURCES})
    target_include_directories(libflowdrop_shared PUBLIC ${LIBFLOWDROP_INCLUDE_DIR})
    target_include_directories(libflowdrop_shared PRIVATE ${LIBFLOWDROP_TARGET_INCLUDE})
    target_link_libraries(libflowdrop_shared PRIVATE ${LIBFLOWDROP_LIBS})
endif ()
if (LIBFLOWDROP_BUILD_STATIC)
    add_library(libflowdrop_static STATIC ${LIBFLOWDROP_SOURCES})
    target_include_directories(libflowdrop_static PUBLIC ${LIBFLOWDROP_INCLUDE_DIR})
    target_include_directories(libflowdrop_static PRIVATE ${LIBFLOWDROP_TARGET_INCLUDE})
    target_link_libraries(libflowdrop_static PRIVATE ${LIBFLOWDROP_LIBS})
endif ()
