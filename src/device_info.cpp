/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#include <string>
#include "../include/flowdrop.hpp"
#include "nlohmann/json.hpp"
#include <iostream>
#include <utility>
#include <sstream>

using json = nlohmann::json;

namespace flowdrop {

    bool debug = false;

    void to_json(json &j, const DeviceInfo &d) {
        j["id"] = d.id;
        if (!d.uuid.empty()) {
            j["uuid"] = d.uuid;
        }
        if (!d.name.empty()) {
            j["name"] = d.name;
        }
        if (!d.model.empty()) {
            j["model"] = d.model;
        }
        if (!d.platform.empty()) {
            j["platform"] = d.platform;
        }
        if (!d.system_version.empty()) {
            j["system_version"] = d.system_version;
        }
    }

    void from_json(const json &j, DeviceInfo &d) {
        j.at("id").get_to(d.id);
        if (j.count("uuid") != 0) {
            j.at("uuid").get_to(d.uuid);
        }
        if (j.count("name") != 0) {
            j.at("name").get_to(d.name);
        }
        if (j.count("model") != 0) {
            j.at("model").get_to(d.model);
        }
        if (j.count("platform") != 0) {
            j.at("platform").get_to(d.platform);
        }
        if (j.count("system_version") != 0) {
            j.at("system_version").get_to(d.system_version);
        }
    }

    std::string gen_md5_id() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        std::ostringstream os;
        os << ts;
        std::string ts_str = os.str();

        std::string hash;
        {
            std::array<uint8_t, 16> md5_hash{};
            std::memset(md5_hash.data(), 0, md5_hash.size());

            std::hash<std::string> hash_fn;
            std::size_t hash_value = hash_fn(ts_str);
            std::memcpy(md5_hash.data(), &hash_value, sizeof(hash_value));

            hash.resize(md5_hash.size() * 2);
            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            for (auto b: md5_hash) {
                ss << std::setw(2) << static_cast<int>(b);
            }
            hash = ss.str();
        }

        return hash.substr(0, 12);
    }

    DeviceInfo thisDeviceInfo{};

    void to_json(json &j, const FileInfo &d) {
        j["name"] = d.name;
        j["size"] = d.size;
    }

    void from_json(const json &j, FileInfo &d) {
        j.at("name").get_to(d.name);
        j.at("size").get_to(d.size);
    }

    void to_json(json &j, const SendAsk &d) {
        j["sender"] = d.sender;
        j["files"] = d.files;
    }

    void from_json(const json &j, SendAsk &d) {
        j.at("sender").get_to(d.sender);
        j.at("files").get_to(d.files);
    }

}

