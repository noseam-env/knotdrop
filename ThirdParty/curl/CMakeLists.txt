# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/noseam-env/libflowdrop/blob/master/LEGAL

option(BUILD_CURL_EXE "" OFF)
option(BUILD_SHARED_LIBS "" OFF)
option(CURL_STATIC_CRT "" OFF) # /MT
option(CURL_ENABLE_SSL "" OFF)

add_subdirectory(curl-8.1.1)
