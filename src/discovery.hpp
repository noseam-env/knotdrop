/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */
#pragma once

namespace discovery {

    void announce(const std::string &id, unsigned short port, const std::function<bool()> &isStopped);

    enum IPType {
        IPv6,
        IPv4
    };

    struct Remote {
        IPType ipType;
        std::string ip;
        unsigned short port;
    };

    using resolveCallback = std::function<void(const std::optional<Remote> &)>;

    void resolveAndQuery(const std::string &id, const resolveCallback &callback);

} // namespace discovery
