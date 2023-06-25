/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#if defined(USE_APPLE_DNS_SD)

#include <dns_sd.h>
#include <iostream>
#include <functional>
#include <utility>
#include <string>
#include "dns_sd_wrapper.hpp"

#if defined(_WIN32)

#include <winsock.h>

#else

#include <sys/select.h>
#include <unistd.h>
#include <netinet/in.h>

#endif

void loop(DNSServiceRef sdRef, std::atomic<bool> &stopFlag) {
    int fd = DNSServiceRefSockFD(sdRef);
#if defined(__linux__)
    while (!stopFlag) {
        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(fd, &readFds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int nfds = select(fd + 1, &readFds, nullptr, nullptr, &timeout);
        if (nfds > 0) {
            if (FD_ISSET(fd, &readFds)) {
                DNSServiceProcessResult(sdRef);
            }
        } else if (nfds < 0) {
            std::cerr << "Error occurred in select" << std::endl;
            break;
        }

        sleep(1);
    }
#else
    while (!stopFlag) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        int nfds = select(fd + 1, &fds, nullptr, nullptr, nullptr);
        if (nfds > 0) {
            DNSServiceProcessResult(sdRef);
        } else {
            break;
        }
    }
#endif
}

void serializeToTXTRecord(TXTRecordRef& txtRecord, const std::unordered_map<std::string, std::string>& txt) {
    for (const auto& pair : txt) {
        std::string key = pair.first;
        std::string value = pair.second;
        TXTRecordSetValue(&txtRecord, key.c_str(), static_cast<uint8_t>(value.length()), value.c_str());
    }
}

void registerService(const char *serviceName, const char *regType, const char *domain, int port, const std::unordered_map<std::string, std::string>& txt, std::atomic<bool> &stopFlag) {
    DNSServiceRef sdRef;

    TXTRecordRef txtRecord;
    TXTRecordCreate(&txtRecord, 0, nullptr);
    serializeToTXTRecord(txtRecord, txt);

    DNSServiceErrorType err = DNSServiceRegister(&sdRef, 0, kDNSServiceInterfaceIndexAny, serviceName, regType, domain,
                                                 nullptr,
                                                 htons(port), TXTRecordGetLength(&txtRecord),
                                                 TXTRecordGetBytesPtr(&txtRecord),
                                                 nullptr, nullptr);

    if (err != kDNSServiceErr_NoError) {
        std::cerr << "DNSServiceRegister failed with error " << err << std::endl;
        return;
    }

    loop(sdRef, stopFlag);

    DNSServiceRefDeallocate(sdRef);
    TXTRecordDeallocate(&txtRecord);
}

void DNSSD_API dnssdBrowseReply(
        DNSServiceRef,
        DNSServiceFlags,
        uint32_t,
        DNSServiceErrorType errorCode,
const char *serviceName,
const char *regType,
const char *replyDomain,
void *context
) {
if (errorCode != kDNSServiceErr_NoError) {
std::cerr << "dnssdBrowseReply failed with error " << errorCode << std::endl;
return;
}

FindReply findReply{serviceName, regType, replyDomain};

auto callback = reinterpret_cast<std::function<void(const FindReply &)> *>(context);
(*callback)(findReply);
}

void findService(const char *regType, const char *domain, std::function<void(const FindReply &)> callback, std::atomic<bool> &stopFlag) {
    DNSServiceRef sdRef;
    DNSServiceErrorType err = DNSServiceBrowse(&sdRef, 0, kDNSServiceInterfaceIndexAny, regType, domain,
                                               dnssdBrowseReply, &callback);
    if (err != kDNSServiceErr_NoError) {
        std::cerr << "DNSServiceBrowse failed with error " << err << std::endl;
    }

    loop(sdRef, stopFlag);

    DNSServiceRefDeallocate(sdRef);
}

std::unordered_map<std::string, std::string> parseTXTRecord(const std::string& txtRecord) {
    std::unordered_map<std::string, std::string> txt;

    size_t startPos = 0;
    size_t endPos = txtRecord.find(' ');
    while (endPos != std::string::npos) {
        std::string pair = txtRecord.substr(startPos, endPos - startPos);
        size_t equalPos = pair.find('=');
        if (equalPos != std::string::npos) {
            std::string key = pair.substr(0, equalPos);
            std::string value = pair.substr(equalPos + 1);
            txt[key] = value;
        }
        startPos = endPos + 1;
        endPos = txtRecord.find(' ', startPos);
    }
    std::string pair = txtRecord.substr(startPos);
    size_t equalPos = pair.find('=');
    if (equalPos != std::string::npos) {
        std::string key = pair.substr(0, equalPos);
        std::string value = pair.substr(equalPos + 1);
        txt[key] = value;
    }

    return txt;
}

void DNSSD_API dnssdResolveReply(
        DNSServiceRef,
        DNSServiceFlags,
        uint32_t,
        DNSServiceErrorType errorCode,
const char *,
const char *hostTarget,
        uint16_t port,
uint16_t txtLen,
const unsigned char *txtRecord,
void *context
) {
if (errorCode != kDNSServiceErr_NoError) {
std::cerr << "dnssdResolveReply failed with error " << errorCode << std::endl;
return;
}

std::string txtString(reinterpret_cast<const char*>(txtRecord), txtLen);

if (!txtString.empty()) {
txtString = txtString.substr(1);
}

auto txt = parseTXTRecord(txtString);

ResolveReply resolveReply{hostTarget, htons(port), txt};

auto callback = reinterpret_cast<std::function<void(const ResolveReply &)> *>(context);
(*callback)(resolveReply);
}

void resolveService(const char *serviceName, const char *regType, const char *domain,
                    std::function<void(const ResolveReply &)> callback) {
    DNSServiceRef sdRef;
    DNSServiceErrorType err = DNSServiceResolve(&sdRef, 0, kDNSServiceInterfaceIndexAny,
                                                serviceName, regType, domain, dnssdResolveReply,
                                                new std::function<void(const ResolveReply &)>(std::move(callback)));
    if (err != kDNSServiceErr_NoError) {
        std::cerr << "DNSServiceResolve failed with error " << err << std::endl;
    }

    DNSServiceProcessResult(sdRef);

    /*
    int fd = DNSServiceRefSockFD(sdRef);
    while (true) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        int nfds = select(fd + 1, &fds, nullptr, nullptr, nullptr);
        if (nfds > 0) {
            DNSServiceProcessResult(sdRef);
        } else {
            break;
        }
    }
     */

    DNSServiceRefDeallocate(sdRef);
}

#endif  // USE_APPLE_DNS_SD
