# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/noseam-env/libflowdrop/blob/master/LEGAL

set(BUILD_CURL_EXE OFF)
set(BUILD_SHARED_LIBS OFF)
set(CURL_ENABLE_SSL OFF)
add_subdirectory(curl-8.1.1)
