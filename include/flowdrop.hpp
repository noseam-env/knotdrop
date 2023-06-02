/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#ifndef LIBFLOWDROP_FLOWDROP_HPP
#define LIBFLOWDROP_FLOWDROP_HPP

#include <string>
#include <functional>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace flowdrop {
    extern bool debug;

    struct DeviceInfo {
        std::string id;
        std::string uuid;
        std::string name;
        std::string model;
        std::string platform;
        std::string system_version;
    };

    void to_json(json &j, const DeviceInfo &d);

    void from_json(const json &j, DeviceInfo &d);

    std::string gen_md5_id();

    extern DeviceInfo thisDeviceInfo;

    struct Address {
        std::string host;
        uint16_t port;
    };

    struct Receiver {
        DeviceInfo deviceInfo{};
        Address address{};
    };

    struct FileInfo {
        std::string name;
        std::int64_t size;
        //long size;
        //std::uintmax_t size;
    };

    void to_json(json &j, const FileInfo &d);

    void from_json(const json &j, FileInfo &d);

    struct SendAsk {
        DeviceInfo sender;
        std::vector<FileInfo> files;
    };

    void to_json(json &j, const SendAsk &d);

    void from_json(const json &j, SendAsk &d);

    using sendAskCallback = std::function<bool(const SendAsk &)>;

    void receive(const std::string &dest, const sendAskCallback &callback);

    using resolveCallback = std::function<void(const Address &)>;

    void resolve(const std::string &id, const resolveCallback &callback);

    using findCallback = std::function<void(const Receiver &)>;

    void find(const findCallback &callback);

    void send(const std::string& receiverId, const std::vector<std::string>& files, int resolveTimeout, int askTimeout);
}


#endif //LIBFLOWDROP_FLOWDROP_HPP
