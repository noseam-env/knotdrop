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
#include <optional>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace flowdrop {
    extern bool debug;

    struct DeviceInfo {
        std::string id;
        std::optional<std::string> uuid;
        std::optional<std::string> name;
        std::optional<std::string> model;
        std::optional<std::string> platform;
        std::optional<std::string> system_version;
    };

    void to_json(json &j, const DeviceInfo &d);

    void from_json(const json &j, DeviceInfo &d);

    std::string gen_md5_id();

    extern DeviceInfo thisDeviceInfo;

    struct FileInfo {
        std::string name;
        std::size_t size;
    };

    void to_json(json &j, const FileInfo &d);

    void from_json(const json &j, FileInfo &d);

    struct SendAsk {
        DeviceInfo sender;
        std::vector<FileInfo> files;
    };

    void to_json(json &j, const SendAsk &d);

    void from_json(const json &j, SendAsk &d);

    class IEventListener {
    public:
        virtual ~IEventListener() = default;

        // sender
        virtual void onResolving() {}
        virtual void onReceiverNotFound() {}
        virtual void onResolved() {}
        virtual void onAskingReceiver() {}
        virtual void onReceiverDeclined() {}
        virtual void onReceiverAccepted() {}
        virtual void onSendingStart() {}
        virtual void onSendingTotalProgress(std::size_t totalSize, std::size_t currentSize) {}
        virtual void onSendingFileStart(const FileInfo &fileInfo) {}
        virtual void onSendingFileProgress(const FileInfo &fileInfo, std::size_t currentSize) {}
        virtual void onSendingFileEnd(const FileInfo &fileInfo) {}
        virtual void onSendingEnd() {}

        // receiver
        virtual void onReceiverStarted(unsigned short port) {}
        virtual void onSenderAsk(const DeviceInfo &sender) {}
        virtual void onReceivingStart(const DeviceInfo &sender, std::size_t totalSize) {}
        virtual void onReceivingTotalProgress(const DeviceInfo &sender, std::size_t totalSize, std::size_t receivedSize) {}
        virtual void onReceivingFileStart(const DeviceInfo &sender, const FileInfo &fileInfo) {}
        virtual void onReceivingFileProgress(const DeviceInfo &sender, const FileInfo &fileInfo, std::size_t receivedSize) {}
        virtual void onReceivingFileEnd(const DeviceInfo &sender, const FileInfo &fileInfo) {}
        virtual void onReceivingEnd(const DeviceInfo &sender, std::size_t totalSize) {}
    };

    using sendAskCallback = std::function<bool(const SendAsk &)>;

    void receive(const std::string &dest, const sendAskCallback &callback, IEventListener *eventListener);

    using findCallback = std::function<void(const DeviceInfo &)>;

    void find(const findCallback &callback);

    void send(const std::string& receiverId, const std::vector<std::string>& files, int resolveTimeout, int askTimeout, IEventListener *eventListener);
}


#endif //LIBFLOWDROP_FLOWDROP_HPP
