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

typedef unsigned short port_t;

port_t knotport_find_open();

#ifdef __cplusplus
} /* end of extern "C" */
#endif
