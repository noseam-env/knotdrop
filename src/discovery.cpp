/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#include <iostream>
#include <set>
#include "dns_sd/dns_sd_wrapper.hpp"
#include "../include/flowdrop.hpp"
#include "discovery.hpp"
#include "specification.hpp"
#include "hv/axios.h"

size_t write_callback(char *contents, size_t size, size_t nmemb, std::string *buffer) {
    size_t totalSize = size * nmemb;
    buffer->append(contents, totalSize);
    return totalSize;
}

void announce(int port) {
    std::unordered_map<std::string, std::string> txt;
    txt["v"] = std::to_string(flowdrop_version);
    registerService(flowdrop::thisDeviceInfo.id.c_str(), flowdrop_reg_type, flowdrop_dns_domain, port, txt);
}

void flowdrop::find(const flowdrop::findCallback &callback) {
    std::set<std::string> foundIds;

    findService(flowdrop_reg_type, flowdrop_dns_domain, [callback, &foundIds](const FindReply &findReply) {
        auto it = foundIds.find(findReply.serviceName);
        if (it != foundIds.end()) {
            return;
        }

        if (debug) {
            std::cout << "found: " << findReply.serviceName << " " << findReply.regType << " "
                      << findReply.replyDomain << std::endl;
        }

        flowdrop::resolve(findReply.serviceName, [callback, &foundIds, &findReply](const Address &address) {
            if (debug) {
                std::cout << "resolved: " << address.host << " " << std::to_string(address.port) << std::endl;
            }

            std::string url = "http://" + address.host + ":" + std::to_string(address.port) + "/device_info";

            auto resp = axios::get(url.c_str());
            if (resp == nullptr) {
                return;
            }

            json jsonData;
            try {
                jsonData = json::parse(resp->body);
            } catch (const std::exception &) {
                return;
            }
            DeviceInfo deviceInfo;
            from_json(jsonData, deviceInfo);

            callback({deviceInfo, address});
        });
    });
}

void flowdrop::resolve(const std::string &id, const flowdrop::resolveCallback &callback) {
    resolveService(id.c_str(), flowdrop_reg_type, flowdrop_dns_domain, [callback](const ResolveReply &resolveReply) {
        std::unordered_map<std::string, std::string> txt = resolveReply.txt;
        if (txt[flowdrop_txt_key_version] != std::to_string(flowdrop_version)) {
            return;
        }
        callback({resolveReply.host, resolveReply.port});
    });
}
