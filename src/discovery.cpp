/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#include <iostream>
#include <set>
#include "dns_sd/dns_sd_wrapper.hpp"
#include "flowdrop.hpp"
#include "discovery.hpp"
#include "specification.hpp"
#include "hv/axios.h"
#include "curl/curl.h"

size_t curl_write_function(void *contents, size_t size, size_t nmemb, std::string *output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char *>(contents), totalSize);
    return totalSize;
}

void announce(const std::string &id, int port, std::atomic<bool> &stopFlag) {
    std::unordered_map<std::string, std::string> txt;
    txt["v"] = std::to_string(flowdrop_version);
    registerService(id.c_str(), flowdrop_reg_type, flowdrop_dns_domain, port, txt, stopFlag);
}

void resolve(const std::string &id, const resolveCallback &callback) {
    resolveService(id.c_str(), flowdrop_reg_type, flowdrop_dns_domain, [callback](const ResolveReply &resolveReply) {
        std::unordered_map<std::string, std::string> txt = resolveReply.txt;
        if (txt[flowdrop_txt_key_version] != std::to_string(flowdrop_version)) {
            return;
        }
        callback({resolveReply.host, resolveReply.port});
    });
}

void flowdrop::find(const flowdrop::findCallback &callback, std::atomic<bool>& stopFlag) {
    std::set<std::string> foundServices;

    findService(flowdrop_reg_type, flowdrop_dns_domain, [callback, &foundServices](const FindReply &findReply) {
        auto it = foundServices.find(findReply.serviceName);
        if (it != foundServices.end()) {
            return;
        }
        foundServices.insert(findReply.serviceName);

        if (debug) {
            std::cout << "found: " << findReply.serviceName << " " << findReply.regType << " "
                      << findReply.replyDomain << std::endl;
        }

        resolve(findReply.serviceName, [callback](const Address &address) {
            if (debug) {
                std::cout << "resolved: " << address.host << " " << std::to_string(address.port) << std::endl;
            }

            curl_global_init(CURL_GLOBAL_NOTHING);

            CURL *curl = curl_easy_init();
            if (!curl) {
                std::cerr << "Failed to initialize curl" << std::endl;
                return;
            }

            std::string url = "http://" + address.host + ":" + std::to_string(address.port) + "/device_info";
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

            std::string response;
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_function);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                //std::cerr << "Failed to execute GET request: " << curl_easy_strerror(res) << std::endl;
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return;
            }

            curl_easy_cleanup(curl);
            curl_global_cleanup();

            json jsonData;
            try {
                jsonData = json::parse(response);
            } catch (const std::exception &) {
                return;
            }
            DeviceInfo deviceInfo;
            from_json(jsonData, deviceInfo);

            callback(deviceInfo);
        });
    }, stopFlag);
}

void flowdrop::find(const flowdrop::findCallback &callback) {
    std::atomic<bool> stopFlag(false);
    flowdrop::find(callback, stopFlag);
}
