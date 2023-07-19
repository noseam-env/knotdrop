/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "portroller.hpp"
#include <memory>

#ifndef MLAS_NO_EXCEPTION
#include <stdexcept>
#endif

#if defined(_WIN32)
#include <WinSock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // maybe unused
#include <unistd.h>
#include <cstring> // maybe unused
#endif

class ServerSocket {
public:
    ServerSocket();
    ~ServerSocket();

    bool bind(const struct sockaddr *addr, unsigned int len);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * Here you can reuse the code, but I plan to make a library to simplify
 * cross-platform and implementations for each platform will be separate
 */

#if defined(_WIN32)

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
    }
    ~Impl() {
        closesocket(sock);
        WSACleanup();
    }

    bool bind(const struct sockaddr *addr, unsigned int len) const {
        return ::bind(sock, addr, (int) len) != SOCKET_ERROR;
    }

private:
    SOCKET sock;
};

ServerSocket::ServerSocket() : pImpl(new Impl) {}
ServerSocket::~ServerSocket() = default;

bool ServerSocket::bind(const struct sockaddr *addr, unsigned int len) {
    return pImpl->bind(addr, len);
}

#else

class ServerSocket::Impl {
public:
    Impl() {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            throw std::runtime_error("Unable to init ServerSocket");
        }
    }
    ~Impl() {
        close(sockfd);
    }

    bool bind(const struct sockaddr *addr, unsigned int len) const {
        return ::bind(sockfd, addr, len) != -1;
    }

private:
    int sockfd;
};

ServerSocket::ServerSocket() : pImpl(new Impl) {}
ServerSocket::~ServerSocket() = default;

bool ServerSocket::bind(const struct sockaddr *addr, unsigned int len) {
    return pImpl->bind(addr, len);
}

#endif

/**
 * portroller code
 */

#include <random>

constexpr unsigned short MIN_PORT = 1024;
constexpr unsigned short MAX_PORT = 65535;
constexpr int MAX_ATTEMPTS = 16;

unsigned short rollAvailablePort(unsigned short defaultPort) {
    auto *serverSocket = new ServerSocket();

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(defaultPort);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(MIN_PORT, MAX_PORT);

    int attempts = 0;
    while (!serverSocket->bind((sockaddr *) &addr, sizeof(addr))) {
        addr.sin_port = htons(distrib(gen));
        attempts++;
        if (attempts >= MAX_ATTEMPTS) {
            delete serverSocket;
            throw std::runtime_error("Failed to find an available port");
        }
    }

    delete serverSocket;
    return ntohs(addr.sin_port);
}
