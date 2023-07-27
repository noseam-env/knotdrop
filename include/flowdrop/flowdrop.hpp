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
#include <fstream>
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
        std::uint64_t size;
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
        virtual void onSendingTotalProgress(std::uint64_t totalSize, std::uint64_t currentSize) {}
        virtual void onSendingFileStart(const FileInfo &fileInfo) {}
        virtual void onSendingFileProgress(const FileInfo &fileInfo, std::uint64_t currentSize) {}
        virtual void onSendingFileEnd(const FileInfo &fileInfo) {}
        virtual void onSendingEnd() {}

        // receiver
        virtual void onReceiverStarted(unsigned short port) {}
        virtual void onSenderAsk(const DeviceInfo &sender) {}
        virtual void onReceivingStart(const DeviceInfo &sender, std::uint64_t totalSize) {}
        virtual void onReceivingTotalProgress(const DeviceInfo &sender, std::uint64_t totalSize, std::uint64_t receivedSize) {}
        virtual void onReceivingFileStart(const DeviceInfo &sender, const FileInfo &fileInfo) {}
        virtual void onReceivingFileProgress(const DeviceInfo &sender, const FileInfo &fileInfo, std::uint64_t receivedSize) {}
        virtual void onReceivingFileEnd(const DeviceInfo &sender, const FileInfo &fileInfo) {}
        virtual void onReceivingEnd(const DeviceInfo &sender, std::uint64_t totalSize) {}
    };

    using askCallback = std::function<bool(const SendAsk &)>;

    class Server {
    public:
        explicit Server(const DeviceInfo &);
        ~Server();

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

    using findCallback = std::function<void(const DeviceInfo &)>;

    void find(const findCallback &callback, const std::function<bool()> &isStopped);

    void find(const findCallback &callback);

    class File {
    public:
        virtual ~File() = default;
        [[nodiscard]] virtual std::string getRelativePath() const = 0;
        [[nodiscard]] virtual std::uint64_t getSize() const = 0;
        [[nodiscard]] virtual std::uint64_t getCreatedTime() const = 0; // UNIX time (__time64_t)
        [[nodiscard]] virtual std::uint64_t getModifiedTime() const = 0; // UNIX time (__time64_t)
        [[nodiscard]] virtual std::filesystem::perms getPermissions() const = 0;
        virtual void seek(std::uint64_t pos) = 0;
        virtual std::uint64_t read(char *buffer, std::uint64_t count) = 0;
    };

    class SendRequest {
    public:
        SendRequest();
        ~SendRequest();

        [[maybe_unused]] [[nodiscard]] DeviceInfo getDeviceInfo() const;
        SendRequest &setDeviceInfo(const DeviceInfo &info);

        [[maybe_unused]] [[nodiscard]] std::string getReceiverId() const;
        SendRequest &setReceiverId(const std::string &id);

        [[maybe_unused]] [[nodiscard]] std::vector<File *> getFiles() const;
        SendRequest& setFiles(const std::vector<File *>& files);

        [[maybe_unused]] [[maybe_unused]] [[nodiscard]] std::chrono::milliseconds getResolveTimeout() const;
        SendRequest &setResolveTimeout(const std::chrono::milliseconds &timeout);

        [[maybe_unused]] [[nodiscard]] std::chrono::milliseconds getAskTimeout() const;
        SendRequest &setAskTimeout(const std::chrono::milliseconds &timeout);

        [[maybe_unused]] [[nodiscard]] IEventListener* getEventListener() const;
        SendRequest &setEventListener(IEventListener *listener);

        bool execute();

    private:
        DeviceInfo deviceInfo;
        std::string receiverId;
        std::vector<File *> files;
        std::chrono::milliseconds resolveTimeout;
        std::chrono::milliseconds askTimeout;
        IEventListener *eventListener;
    };

    class NativeFile : public File {
    public:
        NativeFile(const std::filesystem::path& filePath, std::string relativePath);
        ~NativeFile() override;
        [[nodiscard]] std::string getRelativePath() const override;
        [[nodiscard]] std::uint64_t getSize() const override;
        [[nodiscard]] std::uint64_t getCreatedTime() const override;
        [[nodiscard]] std::uint64_t getModifiedTime() const override;
        [[nodiscard]] std::filesystem::perms getPermissions() const override;
        void seek(std::uint64_t pos) override;
        std::uint64_t read(char *buffer, std::uint64_t count) override;

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };
}

#endif //LIBFLOWDROP_FLOWDROP_HPP
