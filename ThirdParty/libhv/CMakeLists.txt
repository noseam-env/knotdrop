# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/noseam-env/libflowdrop/blob/master/LEGAL

option(BUILD_SHARED "" OFF)
option(BUILD_EXAMPLES "" OFF)
option(WITH_HTTP_CLIENT "" OFF)
option(WITH_MQTT "" OFF)

add_subdirectory(libhv)
