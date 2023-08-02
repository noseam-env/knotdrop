/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "file_info.h"
#include "../logger.h"

#if defined(WIN32)

#include <Windows.h>

std::uint64_t fileTimeToUnixTime(const FILETIME& fileTime) {
    ULARGE_INTEGER largeInt;
    largeInt.LowPart = fileTime.dwLowDateTime;
    largeInt.HighPart = fileTime.dwHighDateTime;

    const std::uint64_t epochOffset = 116444736000000000ULL;
    std::uint64_t fileTimeValue = largeInt.QuadPart - epochOffset;

    const std::uint64_t ticksPerSecond = 10000000ULL;
    return fileTimeValue / ticksPerSecond;
}

void FileInfo::Time(const std::string &filePath, std::uint64_t *ctime, std::uint64_t *mtime) {
    HANDLE fileHandle = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        Logger::log(Logger::LEVEL_ERROR, "getFileTime: Error opening file");
        return;
    }

    FILETIME creationTime, lastAccessTime, lastWriteTime;
    if (GetFileTime(fileHandle, &creationTime, &lastAccessTime, &lastWriteTime)) {
        CloseHandle(fileHandle);
        *ctime = fileTimeToUnixTime(creationTime);
        *mtime = fileTimeToUnixTime(lastWriteTime);
    } else {
        Logger::log(Logger::LEVEL_ERROR, "getFileTime: Error getting file time");
        CloseHandle(fileHandle);
        return;
    }
}

#else

#include <sys/stat.h>

void FileInfo::Time(const std::string &filePath, std::uint64_t *ctime, std::uint64_t *mtime) {
    struct stat fileStat{};
    if (stat(filePath.c_str(), &fileStat) == 0) {
        *ctime = fileStat.st_mtime;
        *mtime = fileStat.st_mtime;
    }
}

#endif


