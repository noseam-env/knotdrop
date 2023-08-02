/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "experimental_socket.h"

#ifndef MLAS_NO_EXCEPTION
#include <stdexcept>
#endif

#if defined(WIN32)

#include <Windows.h>

class ServerSocket::Impl {
public:
    Impl() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("Unable to init WinSock2");
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


