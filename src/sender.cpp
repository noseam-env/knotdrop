/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "flowdrop/flowdrop.hpp"
#include "curl/curl.h"
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <future>
#include "specification.hpp"
#include "virtualtfa.hpp"
#include "discovery.hpp"
#include "logger.h"

size_t writeCallback(char *data, size_t size, size_t nmemb, std::string *response) {
    size_t totalSize = size * nmemb;
    response->append(data, totalSize);
    return totalSize;
}

bool ask(const std::string &baseUrl, const std::vector<flowdrop::FileInfo> &files, const std::chrono::milliseconds &timeout, const flowdrop::DeviceInfo &deviceInfo) {
    flowdrop::SendAsk askData;
    askData.sender = deviceInfo;
    askData.files = files;

    std::string jsonData = json(askData).dump();

    CURL *curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    std::string url = baseUrl + flowdrop_endpoint_ask;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<double>(timeout.count()) / 1000.0);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonData.size());

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        Logger::log(Logger::LEVEL_ERROR, "Ask error: " + std::string(curl_easy_strerror(res)));
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return false;
    }
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    json responseJson;
    try {
        responseJson = json::parse(response);
    } catch (std::exception &) {
        return false;
    }

    return responseJson["accepted"].get<bool>();
}

size_t ignoreDataCallback(char * /*buffer*/, size_t size, size_t nmemb, void * /*userdata*/) {
    return size * nmemb;
}

size_t tfaReadFunction(char *buffer, size_t size, size_t nmemb, void *userdata) {
    Logger::log(Logger::LEVEL_INFO, "2");
    auto *tfa = static_cast<VirtualTfaWriter *>(userdata);
    Logger::log(Logger::LEVEL_INFO, "3");
    return tfa->writeTo(buffer, size * nmemb);
}

class SendProgressListener : public IProgressListener {
public:
    explicit SendProgressListener(flowdrop::IEventListener *eventListener) : m_eventListener(eventListener) {}

    void totalProgress(std::uint64_t currentSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onSendingTotalProgress(m_totalSize, currentSize);
        }
    }

    void fileStart(char *fileName, std::uint64_t fileSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onSendingFileStart({fileName, fileSize});
        }
    }

    void fileProgress(char *fileName, std::uint64_t fileSize, std::uint64_t currentSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onSendingFileProgress({fileName, fileSize}, currentSize);
        }
    }

    void fileEnd(char *fileName, std::uint64_t fileSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onSendingFileEnd({fileName, fileSize});
        }
    }

    void setTotalSize(std::uint64_t totalSize) {
        SendProgressListener::m_totalSize = totalSize;
    }

private:
    flowdrop::IEventListener *m_eventListener;
    std::uint64_t m_totalSize = 0;
};

class FileAdapter : public VirtualFile {
public:
    explicit FileAdapter(flowdrop::File *file) : m_file(file) {}
    ~FileAdapter() override = default;

    [[nodiscard]] std::string getRelativePath() const override {
        return m_file->getRelativePath();
    }
    [[nodiscard]] std::uint64_t getSize() const override {
        return m_file->getSize();
    }
    [[nodiscard]] std::uint64_t getCreatedTime() const override {
        return m_file->getCreatedTime();
    }
    [[nodiscard]] std::uint64_t getModifiedTime() const override {
        return m_file->getModifiedTime();
    }
    [[nodiscard]] std::filesystem::perms getPermissions() const override {
        return m_file->getPermissions();
    }
    void seek(std::uint64_t pos) override {
        m_file->seek(pos);
    }
    std::uint64_t read(char* buffer, std::uint64_t count) override {
        return m_file->read(buffer, count);
    }

private:
    flowdrop::File *m_file;
};

void sendFiles(const std::string &baseUrl, std::vector<flowdrop::File *> &files, flowdrop::IEventListener *listener, const flowdrop::DeviceInfo &deviceInfo) {
    VirtualTfaArchive *archive = virtual_tfa_archive_new();

    for (flowdrop::File *file: files) {
        FileAdapter fileAdapter(file);
        VirtualTfaEntry *entry = virtual_tfa_entry_new(fileAdapter);
        virtual_tfa_archive_add(archive, entry);
    }

    auto *sendProgressListener = new SendProgressListener(listener);
    auto *tfa = new VirtualTfaWriter(archive, sendProgressListener);

    std::uint64_t totalSize = tfa->calcSize();
    sendProgressListener->setTotalSize(totalSize);

    if (listener != nullptr) {
        listener->onSendingStart();
    }

    curl_global_init(CURL_GLOBAL_NOTHING);

    CURL *curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, (baseUrl + flowdrop_endpoint_send).c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_READDATA, tfa);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, tfaReadFunction);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, totalSize);

        struct curl_slist *headers = nullptr;
        std::string header = std::string(flowdrop_deviceinfo_header) + ": " + json(deviceInfo).dump();
        headers = curl_slist_append(headers, header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ignoreDataCallback);

        Logger::log(Logger::LEVEL_INFO, "1");
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            Logger::log(Logger::LEVEL_ERROR, "Send file error: " + std::string(curl_easy_strerror(res)));
        }

        curl_easy_cleanup(curl);
        curl_global_cleanup();
    }

    virtual_tfa_archive_close(archive);
}

bool askAndSend(const discovery::Remote &remote, std::vector<flowdrop::File *> &files, const std::chrono::milliseconds askTimeout,
                flowdrop::IEventListener *listener, const flowdrop::DeviceInfo &deviceInfo) {
    std::string host = remote.ip;
    if (remote.ipType == discovery::IPv6) {
        host = "[" + host + "]";
    }
    std::string baseUrl = "http://" + host + ":" + std::to_string(remote.port) + "/";

    if (listener != nullptr) {
        listener->onAskingReceiver();
    }

    std::vector<flowdrop::FileInfo> filesInfo(files.size());
    for (size_t i = 0; i < files.size(); ++i) {
        filesInfo[i].name = files[i]->getRelativePath();
        filesInfo[i].size = files[i]->getSize();
    }
    if (!ask(baseUrl, filesInfo, askTimeout, deviceInfo)) {
        if (listener != nullptr) {
            listener->onReceiverDeclined();
        }
        return false;
    }

    if (listener != nullptr) {
        listener->onReceiverAccepted();
    }

    sendFiles(baseUrl, files, listener, deviceInfo);

    if (listener != nullptr) {
        listener->onSendingEnd();
    }

    return true;
}

bool send(const std::string &receiverId, std::vector<flowdrop::File *> &files,
          const std::chrono::milliseconds &resolveTimeout, const std::chrono::milliseconds &askTimeout,
          flowdrop::IEventListener *listener, const flowdrop::DeviceInfo &deviceInfo) {
    if (listener != nullptr) {
        listener->onResolving();
    }

    std::promise<std::optional<discovery::Remote>> resolvePromise;
    std::future<std::optional<discovery::Remote>> resolveFuture = resolvePromise.get_future();

    std::thread resolveThread([receiverId, &resolvePromise]() {
        discovery::resolveAndQuery(receiverId, [&resolvePromise](const std::optional<discovery::Remote> &remoteOpt) {
            resolvePromise.set_value(remoteOpt);
        });
    });

    std::future_status status = resolveFuture.wait_for(resolveTimeout);

    bool result;
    if (status == std::future_status::ready) {
        std::optional<discovery::Remote> remoteOpt = resolveFuture.get();
        if (!remoteOpt.has_value()) {
            if (listener != nullptr) {
                listener->onReceiverNotFound();
            }
            return false;
        }
        if (listener != nullptr) {
            listener->onResolved();
        }
        discovery::Remote remote = remoteOpt.value();
        Logger::log(Logger::LEVEL_DEBUG, "fully resolved: " + remote.ip + ":" + std::to_string(remote.port));
        result = askAndSend(remote, files, askTimeout, listener, deviceInfo);
    } else {
        if (listener != nullptr) {
            listener->onReceiverNotFound();
        }
        return false;
    }

    resolveThread.join();
    return result;
}

namespace flowdrop {
    SendRequest::SendRequest() = default;
    SendRequest::~SendRequest() = default;

    SendRequest& SendRequest::setDeviceInfo(const DeviceInfo& info) {
        deviceInfo = info;
        return *this;
    }

    SendRequest& SendRequest::setReceiverId(const std::string& id) {
        receiverId = id;
        return *this;
    }

    SendRequest& SendRequest::setFiles(const std::vector<File *>& files) {
        this->files = files;
        return *this;
    }

    SendRequest& SendRequest::setResolveTimeout(const std::chrono::milliseconds& timeout) {
        resolveTimeout = timeout;
        return *this;
    }

    SendRequest& SendRequest::setAskTimeout(const std::chrono::milliseconds& timeout) {
        askTimeout = timeout;
        return *this;
    }

    SendRequest& SendRequest::setEventListener(IEventListener* listener) {
        eventListener = listener;
        return *this;
    }

    [[maybe_unused]] DeviceInfo SendRequest::getDeviceInfo() const {
        return deviceInfo;
    }

    std::string SendRequest::getReceiverId() const {
        return receiverId;
    }

    std::vector<File *> SendRequest::getFiles() const {
        return files;
    }

    std::chrono::milliseconds SendRequest::getResolveTimeout() const {
        return resolveTimeout;
    }

    std::chrono::milliseconds SendRequest::getAskTimeout() const {
        return askTimeout;
    }

    IEventListener* SendRequest::getEventListener() const {
        return eventListener;
    }

    bool SendRequest::execute() {
        return send(receiverId, files, resolveTimeout, askTimeout, eventListener, deviceInfo);
    }
}
