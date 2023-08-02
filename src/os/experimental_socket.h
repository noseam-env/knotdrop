/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */
#pragma once

#include <cstdint>
#include <memory>

class ServerSocket {
public:
    ServerSocket();
    ~ServerSocket();

    bool bind(unsigned short port);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};
