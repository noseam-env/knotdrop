/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#include <random>
#include "portroller.hpp"
#include "specification.hpp"

#if defined(_WIN32)

#include <winsock2.h>

#elif defined(__linux__) || defined(__APPLE__)

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#endif

const int MIN_PORT = 1000;
const int MAX_PORT = 65500;

void randomizePort(sockaddr_in addr) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(MIN_PORT, MAX_PORT);
    int newPort = distrib(gen);
    addr.sin_port = htons(newPort);
}

unsigned short rollAvailablePort() {
#if defined(_WIN32)
    // initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return -1;
    }

    // create a socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return -1;
    }

    // bind the socket to a local address and port
    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(flowdrop_default_port);
    while (bind(sock, (sockaddr *) &addr, sizeof(addr)) == SOCKET_ERROR) {
        randomizePort(addr);
    }

    // release the socket and clean up Winsock
    closesocket(sock);
    WSACleanup();

    return ntohs(addr.sin_port);
#else
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return false;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(flowdrop_default_port);
    while (bind(sockfd, (sockaddr *) &addr, sizeof(addr)) == -1) {
        randomizePort(addr);
    }

    close(sockfd);
    return ntohs(addr.sin_port);
#endif
}
