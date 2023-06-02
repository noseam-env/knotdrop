# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL

cmake_minimum_required(VERSION 3.12)
project(libarchive)

if (WIN32)
    add_library(archive_static STATIC IMPORTED GLOBAL)
    target_include_directories(archive_static INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/win64/include")
    set_target_properties(archive_static PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/win64/lib/archive_static.lib")

    target_link_libraries(archive_static PRIVATE INTERFACE bzip2)
    target_link_libraries(archive_static PRIVATE INTERFACE xz)
    target_link_libraries(archive_static PRIVATE INTERFACE libzstd_static)
    target_link_libraries(archive_static PRIVATE INTERFACE external_zlib)
else ()
    set(ENABLE_OPENSSL OFF)
    set(ENABLE_LZMA OFF)
    set(ENABLE_ZLIB OFF)
    set(ENABLE_BZip2 OFF)
    set(ENABLE_CNG OFF)
    set(ENABLE_CPIO OFF)
    set(ENABLE_CAT OFF)
    set(ENABLE_XATTR OFF)

    add_subdirectory(libarchive-3.6.2)
endif ()
