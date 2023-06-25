/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#include <utility>
#include <sstream>
#include "flowdrop.hpp"

void getJsonOptionalString(const json &j, const std::string &key, std::optional<std::string> &strOpt) {
    if (j.count(key) != 0) {
        std::string strValue;
        j.at(key).get_to(strValue);
        if (!strValue.empty()) {
            strOpt = std::move(strValue);
        }
    }
}

void setJsonOptionalString(json &j, const std::string &key, const std::optional<std::string> &strOpt) {
    if (strOpt.has_value()) {
        j[key] = *strOpt;
    }
}

namespace flowdrop {

    bool debug = false;

    std::string generate_md5_id() {
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

    void to_json(json &j, const DeviceInfo &d) {
        j["id"] = d.id;
        setJsonOptionalString(j, "uuid", d.uuid);
        setJsonOptionalString(j, "name", d.name);
        setJsonOptionalString(j, "model", d.model);
        setJsonOptionalString(j, "platform", d.platform);
        setJsonOptionalString(j, "system_version", d.system_version);
    }

    void from_json(const json &j, DeviceInfo &d) {
        j.at("id").get_to(d.id);
        getJsonOptionalString(j, "uuid", d.uuid);
        getJsonOptionalString(j, "name", d.name);
        getJsonOptionalString(j, "model", d.model);
        getJsonOptionalString(j, "platform", d.platform);
        getJsonOptionalString(j, "system_version", d.system_version);
    }

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
