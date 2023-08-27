/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

int knotdrop_util_fileinfo(const char *filePath, uint64_t *ctime, uint64_t *mtime);

#ifdef __cplusplus
} /* end of extern "C" */
#endif
