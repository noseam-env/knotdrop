# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL

set(ZSTD_BUILD_SHARED OFF)
set(ZSTD_BUILD_STATIC ON)
add_subdirectory(zstd-1.5.5/build/cmake)
