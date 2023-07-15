/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
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

    std::string generate_md5_id();

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

    using findCallback = std::function<void(const DeviceInfo &)>;

    void find(const findCallback &callback, std::atomic<bool>& stopFlag);

    void find(const findCallback &callback);

    class SendRequest {
    public:
        SendRequest();
        ~SendRequest();

        [[nodiscard]] DeviceInfo getDeviceInfo() const;
        SendRequest &setDeviceInfo(const DeviceInfo &info);

        [[nodiscard]] std::string getReceiverId() const;
        SendRequest &setReceiverId(const std::string &id);

        [[nodiscard]] std::vector<std::string> getFiles() const;
        SendRequest& setFiles(const std::vector<std::string>& files);

        [[nodiscard]] std::chrono::milliseconds getResolveTimeout() const;
        SendRequest &setResolveTimeout(const std::chrono::milliseconds &timeout);

        [[nodiscard]] std::chrono::milliseconds getAskTimeout() const;
        SendRequest &setAskTimeout(const std::chrono::milliseconds &timeout);

        [[nodiscard]] IEventListener* getEventListener() const;
        SendRequest &setEventListener(IEventListener *listener);

        bool execute();

    private:
        DeviceInfo deviceInfo;
        std::string receiverId;
        std::vector<std::string> files;
        std::chrono::milliseconds resolveTimeout;
        std::chrono::milliseconds askTimeout;
        IEventListener *eventListener;
    };

    using askCallback = std::function<bool(const SendAsk &)>;

    class Receiver {
    public:
        Receiver(const DeviceInfo &);
        ~Receiver();

        [[nodiscard]] const DeviceInfo &getDeviceInfo() const;

        void setDestDir(const std::filesystem::path &);
        [[nodiscard]] const std::filesystem::path &getDestDir() const;

        void setAskCallback(const askCallback &);
        [[nodiscard]] const askCallback &getAskCallback() const;

        void setEventListener(IEventListener *);
        IEventListener *getEventListener();

        void run(bool wait = true);
        void stop();

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };
}


#endif //LIBFLOWDROP_FLOWDROP_HPP
