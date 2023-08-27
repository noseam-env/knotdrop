/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "file_info.h"

#if defined(WIN32)

#include <Windows.h>

uint64_t knotdrop_util_filetime_to_unixtime(FILETIME fileTime) {
    ULARGE_INTEGER largeInt;
    largeInt.LowPart = fileTime.dwLowDateTime;
    largeInt.HighPart = fileTime.dwHighDateTime;

    const uint64_t epochOffset = 116444736000000000ULL;
    uint64_t fileTimeValue = largeInt.QuadPart - epochOffset;

    const uint64_t ticksPerSecond = 10000000ULL;
    return fileTimeValue / ticksPerSecond;
}

int knotdrop_util_fileinfo(const char *filePath, uint64_t *ctime, uint64_t *mtime) {
    HANDLE fileHandle = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return 1;
    }

    FILETIME creationTime, lastAccessTime, lastWriteTime;
    if (GetFileTime(fileHandle, &creationTime, &lastAccessTime, &lastWriteTime)) {
        CloseHandle(fileHandle);
        *ctime = knotdrop_util_filetime_to_unixtime(creationTime);
        *mtime = knotdrop_util_filetime_to_unixtime(lastWriteTime);
    } else {
        CloseHandle(fileHandle);
        return 2;
    }
    return 0;
}

#else

#include <sys/stat.h>

int knotdrop_util_fileinfo(const char *filePath, uint64_t *ctime, uint64_t *mtime) {
    struct stat fileStat;
    if (stat(filePath, &fileStat) != 0) {
        return 1;
    }
    *ctime = (uint64_t) fileStat.st_ctime;
    *mtime = (uint64_t) fileStat.st_mtime;
    return 0;
}

#endif


