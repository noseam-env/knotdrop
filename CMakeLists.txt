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
        include/flowdrop.hpp
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
        src/libarchive.h
        # may used in future
        #src/pipe.cpp
        #src/pipe.hpp
        )

set(LIBFLOWDROP_LIBS
        libcurl
        hv_static
        archive_static
        nlohmann_json::nlohmann_json)

set(LIBFLOWDROP_TARGET_INCLUDE ${LIBHV_HEADERS})

add_subdirectory(ThirdParty/curl)
add_subdirectory(ThirdParty/libarchive)
add_subdirectory(ThirdParty/libhv)
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
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MT")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MT")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MDd")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MDd")

    list(APPEND LIBFLOWDROP_LIBS ws2_32 ntdll)
endif ()

if (UNIX)
    set_property(TARGET libcurl PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET hv_static PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()

if (LIBFLOWDROP_BUILD_SHARED)
    add_library(libflowdrop_shared SHARED ${LIBFLOWDROP_SOURCES})
    target_include_directories(libflowdrop_shared PRIVATE ${LIBFLOWDROP_TARGET_INCLUDE})
    target_link_libraries(libflowdrop_shared PRIVATE ${LIBFLOWDROP_LIBS})
endif ()
if (LIBFLOWDROP_BUILD_STATIC)
    add_library(libflowdrop_static STATIC ${LIBFLOWDROP_SOURCES})
    target_include_directories(libflowdrop_static PRIVATE ${LIBFLOWDROP_TARGET_INCLUDE})
    target_link_libraries(libflowdrop_static PRIVATE ${LIBFLOWDROP_LIBS})
endif ()
