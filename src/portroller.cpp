/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "portroller.hpp"
#include "os/experimental_socket.h"
#include <random>
#include <stdexcept>

constexpr unsigned short MIN_PORT = 1024;
constexpr unsigned short MAX_PORT = 65535;
#if defined(ANDROID)
constexpr int MAX_ATTEMPTS = 64; // I do not know why it works like this
#else
constexpr int MAX_ATTEMPTS = 16;
#endif

unsigned short rollAvailablePort(unsigned short defaultPort) {
    auto *serverSocket = new ServerSocket();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(MIN_PORT, MAX_PORT);

    unsigned short port = defaultPort;

    int attempts = 0;
    while (!serverSocket->bind(port)) {
        port = distrib(gen);
        attempts++;
        if (attempts >= MAX_ATTEMPTS) {
            delete serverSocket;
            throw std::runtime_error("Failed to find an available port");
        }
    }

    delete serverSocket;
    return port;
}
