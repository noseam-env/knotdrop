/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */
#pragma once

#include <cstdint>
#include <string>
#include <stdexcept>

namespace FileInfo {
    void Time(const std::string &filePath, std::uint64_t *ctime, std::uint64_t *mtime);
}
