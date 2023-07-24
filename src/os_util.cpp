/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "os_util.h"

#ifndef MLAS_NO_EXCEPTION
#include <stdexcept>
#endif

#if defined(WIN32)

#include <Windows.h>
#include <iostream>

std::uint64_t fileTimeToUnixTime(const FILETIME& fileTime) {
    ULARGE_INTEGER largeInt;
    largeInt.LowPart = fileTime.dwLowDateTime;
    largeInt.HighPart = fileTime.dwHighDateTime;

    const std::uint64_t epochOffset = 116444736000000000ULL;
    std::uint64_t fileTimeValue = largeInt.QuadPart - epochOffset;

    const std::uint64_t ticksPerSecond = 10000000ULL;
    return fileTimeValue / ticksPerSecond;
}

void getFileTime(const char *filePath, std::uint64_t *ctime, std::uint64_t *mtime) {
    HANDLE fileHandle = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "getFileTime: Error opening file" << std::endl;
        return;
    }

    FILETIME creationTime, lastAccessTime, lastWriteTime;
    if (GetFileTime(fileHandle, &creationTime, &lastAccessTime, &lastWriteTime)) {
        CloseHandle(fileHandle);
        *ctime = fileTimeToUnixTime(creationTime);
        *mtime = fileTimeToUnixTime(lastWriteTime);
    } else {
        std::cerr << "getFileTime: Error getting file time" << std::endl;
        CloseHandle(fileHandle);
        return;
    }
}

class ServerSocket::Impl {
public:
    Impl() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("Unable to init ServerSocket");
        }

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("Unable to init ServerSocket");
        }

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    ~Impl() {
        closesocket(sock);
        WSACleanup();
    }

    [[nodiscard]] bool bind(unsigned short port) {
        addr.sin_port = htons(port);
        return ::bind(sock, (sockaddr *) &addr, sizeof(addr)) != SOCKET_ERROR;
    }

private:
    SOCKET sock;
    sockaddr_in addr{};
};

ServerSocket::ServerSocket() : pImpl(new Impl) {}
ServerSocket::~ServerSocket() = default;

bool ServerSocket::bind(unsigned short port) {
    return pImpl->bind(port);
}

#else

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

void getFileTime(const char *filePath, std::uint64_t *ctime, std::uint64_t *mtime) {
    // TODO: implement
}

class ServerSocket::Impl {
public:
    Impl() {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            throw std::runtime_error("Unable to init ServerSocket");
        }

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    ~Impl() {
        close(sockfd);
    }

    [[nodiscard]] bool bind(unsigned short port) {
        addr.sin_port = htons(port);
        return ::bind(sockfd, (sockaddr *) &addr, sizeof(addr)) != -1;
    }

private:
    int sockfd;
    sockaddr_in addr{};
};

ServerSocket::ServerSocket() : pImpl(new Impl) {}
ServerSocket::~ServerSocket() = default;

bool ServerSocket::bind(unsigned short port) {
    return pImpl->bind(port);
}

#endif


