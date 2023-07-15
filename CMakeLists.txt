# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/noseam-env/libflowdrop/blob/master/LEGAL

cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

cmake_policy(SET CMP0077 NEW)

project(libflowdrop)

option(LIBFLOWDROP_BUILD_STATIC "BUILD STATIC LIBRARIES" ON)
option(LIBFLOWDROP_BUILD_SHARED "BUILD SHARED LIBRARIES" ON)
option(ENABLE_KNOT_DNSSD "ENABLE KNOT DNS-SD" ON)

set(CMAKE_CXX_STANDARD 17)

set(LIBFLOWDROP_SOURCES
        src/discovery.cpp
        src/discovery.hpp
        src/portroller.cpp
        src/portroller.hpp
        src/receiver.cpp
        src/sender.cpp
        src/specification.hpp
        src/core.cpp)

set(LIBFLOWDROP_PUBLIC_LIBS
        nlohmann_json::nlohmann_json)

set(LIBFLOWDROP_PRIVATE_LIBS
        libcurl
        hv_static
        virtualtfa_static)

if (ENABLE_KNOT_DNSSD)
    list(APPEND LIBFLOWDROP_LIBS knotdnssd_static)
endif ()

set(LIBFLOWDROP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(LIBFLOWDROP_TARGET_INCLUDE ${LIBHV_HEADERS})

set(VIRTUALTFA_BUILD_SHARED OFF)

add_subdirectory(ThirdParty/curl)
add_subdirectory(ThirdParty/libhv)
if (ENABLE_KNOT_DNSSD)
    add_subdirectory(ThirdParty/libknotdnssd)
endif ()
add_subdirectory(ThirdParty/libvirtualtfa)
add_subdirectory(ThirdParty/nlohmann_json)

if (WIN32)
    option(BUILD_FOR_MT "build for /MT" OFF)
    if (BUILD_FOR_MT)
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
    endif ()

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
    target_link_libraries(libflowdrop_shared PUBLIC ${LIBFLOWDROP_PUBLIC_LIBS})
    target_link_libraries(libflowdrop_shared PRIVATE ${LIBFLOWDROP_PRIVATE_LIBS})
endif ()
if (LIBFLOWDROP_BUILD_STATIC)
    add_library(libflowdrop_static STATIC ${LIBFLOWDROP_SOURCES})
    target_include_directories(libflowdrop_static PUBLIC ${LIBFLOWDROP_INCLUDE_DIR})
    target_include_directories(libflowdrop_static PRIVATE ${LIBFLOWDROP_TARGET_INCLUDE})
    target_link_libraries(libflowdrop_static PUBLIC ${LIBFLOWDROP_PUBLIC_LIBS})
    target_link_libraries(libflowdrop_static PRIVATE ${LIBFLOWDROP_PRIVATE_LIBS})
endif ()
