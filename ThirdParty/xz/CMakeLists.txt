# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL

cmake_minimum_required(VERSION 3.12)
project(xz)

if (WIN32)
    add_library(xz STATIC IMPORTED GLOBAL)
    target_include_directories(xz INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/win/include")
    set_target_properties(xz PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/win/bin_x86-64/liblzma.a")
else ()

endif ()
