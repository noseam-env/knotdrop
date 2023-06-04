/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#ifndef LIBFLOWDROP_DNS_SD_WRAPPER_HPP
#define LIBFLOWDROP_DNS_SD_WRAPPER_HPP

#include <functional>
#include <unordered_map> // required on Android

void registerService(const char *serviceName, const char *regType, const char *domain, int port, const std::unordered_map<std::string, std::string>& txt);

struct FindReply {
    const char* serviceName;
    const char* regType;
    const char* replyDomain;
};

void findService(const char *regType, const char *domain, std::function<void(const FindReply&)> callback);

struct ResolveReply {
    const char* host;
    uint16_t port;
    std::unordered_map<std::string, std::string> txt;
};

void resolveService(const char *serviceName, const char *regType, const char *domain, std::function<void(const ResolveReply&)> callback);

#endif //LIBFLOWDROP_DNS_SD_WRAPPER_HPP
