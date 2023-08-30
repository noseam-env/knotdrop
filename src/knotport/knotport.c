/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "knotport.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2ipdef.h>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

void knotport_util_close(int sock_ipv4, int sock_ipv6) {
#ifdef _WIN32
    closesocket(sock_ipv4);
    closesocket(sock_ipv6);
    WSACleanup();
#else
    close(sock_ipv4);
    close(sock_ipv6);
#endif
}

port_t knotport_find_open() {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        perror("WSAStartup failed");
        return 0;
    }
#endif

    int sock_ipv4 = socket(AF_INET, SOCK_STREAM, 0);
    int sock_ipv6 = socket(AF_INET6, SOCK_STREAM, 0);

    if (sock_ipv4 == -1 || sock_ipv6 == -1) {
        perror("Socket creation failed");
        return 0;
    }

    struct sockaddr_in addr_ipv4;
    struct sockaddr_in6 addr_ipv6;

    memset(&addr_ipv4, 0, sizeof(addr_ipv4));
    memset(&addr_ipv6, 0, sizeof(addr_ipv6));

    addr_ipv4.sin_family = AF_INET;
    addr_ipv4.sin_addr.s_addr = INADDR_ANY;

    addr_ipv6.sin6_family = AF_INET6;
    addr_ipv6.sin6_addr = in6addr_any;

    // 0 means choose a random available port
    addr_ipv4.sin_port = htons(0);
    addr_ipv6.sin6_port = htons(0);

    if (bind(sock_ipv4, (struct sockaddr*)&addr_ipv4, sizeof(addr_ipv4)) < 0) {
        perror("IPv4 bind failed");
        knotport_util_close(sock_ipv4, sock_ipv6);
        return 0;
    }

    if (bind(sock_ipv6, (struct sockaddr*)&addr_ipv6, sizeof(addr_ipv6)) < 0) {
        perror("IPv6 bind failed");
        knotport_util_close(sock_ipv4, sock_ipv6);
        return 0;
    }

    port_t port;
    socklen_t len;

    len = sizeof(addr_ipv4);
    if (getsockname(sock_ipv4, (struct sockaddr*)&addr_ipv4, &len) == -1) {
        perror("Error getting IPv4 port");
    } else {
        port = ntohs(addr_ipv4.sin_port);
    }

    len = sizeof(addr_ipv6);
    if (getsockname(sock_ipv6, (struct sockaddr*)&addr_ipv6, &len) == -1) {
        perror("Error getting IPv6 port");
    } else {
        port = ntohs(addr_ipv6.sin6_port);
    }

    knotport_util_close(sock_ipv4, sock_ipv6);

    return port;
}
