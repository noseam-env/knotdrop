# This file is part of libflowdrop.
#
# For license and copyright information please follow this link:
# https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL

cmake_minimum_required(VERSION 3.12)
project(bzip2)

set(SOURCES
        bzip2-1.0.8/blocksort.c
        bzip2-1.0.8/huffman.c
        bzip2-1.0.8/crctable.c
        bzip2-1.0.8/randtable.c
        bzip2-1.0.8/compress.c
        bzip2-1.0.8/decompress.c
        bzip2-1.0.8/bzlib.c
        )

set(HEADERS
        bzip2-1.0.8/bzlib.h
        )

add_library(bzip2 STATIC ${SOURCES} ${HEADERS})
