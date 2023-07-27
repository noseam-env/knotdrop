/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */
#pragma once

namespace discovery {

    void announce(const std::string &id, int port, const std::function<bool()> &isStopped);

    struct Address {
        std::string host;
        unsigned short port;
    };

    using resolveCallback = std::function<void(const Address &)>;

    void resolve(const std::string &id, const resolveCallback &callback);

} // namespace discovery
