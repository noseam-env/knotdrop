# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL

set(ENABLE_OPENSSL OFF)
set(ENABLE_LZMA OFF)
set(ENABLE_ZSTD OFF)
set(ENABLE_ZLIB OFF)
set(ENABLE_BZip2 OFF)
set(ENABLE_PCREPOSIX OFF)
set(ENABLE_CNG OFF)
set(ENABLE_CPIO OFF)
set(ENABLE_CAT OFF)
set(ENABLE_XATTR OFF)

if (MSVC)
    add_definitions(-DHAVE_WCSCPY)
    add_definitions(-DHAVE_WCSLEN)
endif ()

add_subdirectory(libarchive-3.6.2)
target_include_directories(archive_static PUBLIC libarchive-3.6.2/libarchive)
