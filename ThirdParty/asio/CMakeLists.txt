# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL

cmake_minimum_required(VERSION 3.12)
project(asio)

set(ASIO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asio-1.28.0/asio")
set(ASIO_INCLUDE_DIR "${ASIO_SOURCE_DIR}/include")
add_library(asio INTERFACE ${ASIO_SOURCE_DIR}/src/asio.cpp ${ASIO_SOURCE_DIR}/src/asio_ssl.cpp)
target_include_directories(asio INTERFACE ${ASIO_INCLUDE_DIR})
