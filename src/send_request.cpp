/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "flowdrop/flowdrop.hpp"
#include "core.h"
#include "curl/curl.h"
#include <vector>
#include <string>
#include <thread>
#include <future>
#include "specification.h"
#include "virtualtfa.h"
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

size_t tfaWriterReadFunc(char *buffer, size_t size, size_t nmemb, void *userdata) {
    auto *tfa = static_cast<virtual_tfa_writer *>(userdata);
    size_t bytes_written = 0;
    int result = virtual_tfa_writer_write(tfa, buffer, size * nmemb, &bytes_written);
    if (result != 0) {
        Logger::log(Logger::LEVEL_ERROR, "failed to read archive, code: " + std::to_string(result));
    }
    return bytes_written;
}

class SendProgressListener {
public:
    explicit SendProgressListener(flowdrop::IEventListener *eventListener) : _eventListener(eventListener) {}

    void totalProgress(tfa_size_t currentSize) {
        if (_eventListener != nullptr) {
            _eventListener->onSendingTotalProgress(_totalSize, currentSize);
        }
    }

    void fileStart(const virtual_tfa_file_info *fileInfo) {
        if (_eventListener != nullptr) {
            _eventListener->onSendingFileStart({fileInfo->name, fileInfo->size});
        }
    }

    void fileProgress(const virtual_tfa_file_info *fileInfo, tfa_size_t currentSize) {
        if (_eventListener != nullptr) {
            _eventListener->onSendingFileProgress({fileInfo->name, fileInfo->size}, currentSize);
        }
    }

    void fileEnd(const virtual_tfa_file_info *fileInfo) {
        if (_eventListener != nullptr) {
            _eventListener->onSendingFileEnd({fileInfo->name, fileInfo->size});
        }
    }

    void setTotalSize(tfa_size_t totalSize) {
        _totalSize = totalSize;
    }

private:
    flowdrop::IEventListener *_eventListener;
    tfa_size_t _totalSize = 0;
};

namespace send_request_listener {
    void total_progress(void *userdata, tfa_size_t currentSize) {
        auto *listener = static_cast<SendProgressListener *>(userdata);
        listener->totalProgress(currentSize);
    }

    void file_start(void *userdata, const virtual_tfa_file_info *fileInfo) {
        auto *listener = static_cast<SendProgressListener *>(userdata);
        listener->fileStart(fileInfo);
    }

    void file_progress(void *userdata, const virtual_tfa_file_info *fileInfo, tfa_size_t currentSize) {
        auto *listener = static_cast<SendProgressListener *>(userdata);
        listener->fileProgress(fileInfo, currentSize);
    }

    void file_end(void *userdata, const virtual_tfa_file_info *fileInfo) {
        auto *listener = static_cast<SendProgressListener *>(userdata);
        listener->fileEnd(fileInfo);
    }
}

tfa_size_t streamReadFunc(void *userdata, char *buffer, tfa_size_t size) {
    auto *file = static_cast<flowdrop::File *>(userdata);
    return file->read(buffer, size);
}

void streamCloseFunc(void *userdata) {
    auto *file = static_cast<flowdrop::File *>(userdata);
    delete file;
}

virtual_tfa_input_stream *streamSupplier(void *userdata) {
    virtual_tfa_input_stream *input_stream = virtual_tfa_input_stream_new();

    virtual_tfa_input_stream_set_read_function(input_stream, streamReadFunc);
    virtual_tfa_input_stream_set_read_userdata(input_stream, userdata);
    virtual_tfa_input_stream_set_close_function(input_stream, streamCloseFunc);
    virtual_tfa_input_stream_set_close_userdata(input_stream, userdata);

    return input_stream;
}

void sendFiles(const std::string &baseUrl, std::vector<flowdrop::File *> &files, flowdrop::IEventListener *listener, const flowdrop::DeviceInfo &deviceInfo) {
    virtual_tfa_archive *tfa_archive = virtual_tfa_archive_new();
    if (!tfa_archive) {
        Logger::log(Logger::LEVEL_ERROR, "Failed to initialize virtual_tfa_archive");
        return;
    }

    for (flowdrop::File *file: files) {
        virtual_tfa_entry *entry = virtual_tfa_entry_new();
        if (!entry) {
            Logger::log(Logger::LEVEL_ERROR, "Failed to initialize virtual_tfa_entry");
            virtual_tfa_archive_free(tfa_archive);
            return;
        }

        const std::string& relativePath = file->getRelativePath();
        const char* name = relativePath.c_str();

        char* nameBuffer = new char[1024];
        strcpy(nameBuffer, name);

        virtual_tfa_entry_set_name(entry, nameBuffer);

        virtual_tfa_entry_set_size(entry, file->getSize());
        virtual_tfa_entry_set_input_stream_supplier(entry, streamSupplier);
        virtual_tfa_entry_set_input_stream_supplier_userdata(entry, file);
        virtual_tfa_entry_set_ctime(entry, file->getCreatedTime());
        virtual_tfa_entry_set_mtime(entry, file->getModifiedTime());
        virtual_tfa_entry_set_mode(entry, static_cast<tfa_mode_t>(file->getPermissions()));

        virtual_tfa_archive_add(tfa_archive, entry);
    }

    SendProgressListener *progressListener;
    virtual_tfa_listener *tfa_listener;
    if (listener != nullptr) {
        progressListener = new SendProgressListener(listener);
        void *userdata = static_cast<void *>(progressListener);

        tfa_listener = new virtual_tfa_listener{
            send_request_listener::total_progress,
            userdata,
            send_request_listener::file_start,
            userdata,
            send_request_listener::file_progress,
            userdata,
            send_request_listener::file_end,
            userdata
        };
    } else {
        progressListener = nullptr;
        tfa_listener = nullptr;
    }

    virtual_tfa_writer *tfa_writer = virtual_tfa_writer_new();
    if (!tfa_writer) {
        Logger::log(Logger::LEVEL_ERROR, "Failed to initialize virtual_tfa_writer");
        virtual_tfa_archive_free(tfa_archive);
        return;
    }

    virtual_tfa_writer_set_archive(tfa_writer, tfa_archive);
    virtual_tfa_writer_set_listener(tfa_writer, tfa_listener);

    tfa_size_t totalSize = virtual_tfa_writer_calc_size(tfa_writer);
    Logger::log(Logger::LEVEL_DEBUG, "tfa size: " + std::to_string(totalSize));

    if (progressListener != nullptr) {
        progressListener->setTotalSize(totalSize);
    }

    if (listener != nullptr) {
        listener->onSendingStart();
    }

    curl_global_init(CURL_GLOBAL_NOTHING);

    CURL *curl = curl_easy_init();
    if (!curl) {
        virtual_tfa_writer_free(tfa_writer);
        virtual_tfa_archive_free(tfa_archive);
        delete tfa_listener;
        delete progressListener;
        Logger::log(Logger::LEVEL_ERROR, "Failed to initialize curl");
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, (baseUrl + flowdrop_endpoint_send).c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_READDATA, tfa_writer);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, tfaWriterReadFunc);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, totalSize);

    std::string header = std::string(flowdrop_deviceinfo_header) + ": " + json(deviceInfo).dump();
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, header.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ignoreDataCallback);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        Logger::log(Logger::LEVEL_ERROR, "Send file error: " + std::string(curl_easy_strerror(res)));
    }

    curl_easy_cleanup(curl);

    curl_global_cleanup();

    virtual_tfa_writer_free(tfa_writer);
    virtual_tfa_archive_free(tfa_archive);

    delete tfa_listener;
    delete progressListener;
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

    if (status != std::future_status::ready) {
        if (listener != nullptr) {
            listener->onReceiverNotFound();
        }
        resolveThread.join();
        return false;
    }

    try {
        std::optional<discovery::Remote> remoteOpt = resolveFuture.get();
        if (!remoteOpt.has_value()) {
            if (listener != nullptr) {
                listener->onReceiverNotFound();
            }
            resolveThread.join();
            return false;
        }
        if (listener != nullptr) {
            listener->onResolved();
        }
        discovery::Remote remote = remoteOpt.value();
        resolveThread.join();
        Logger::log(Logger::LEVEL_DEBUG, "fully resolved: " + remote.ip + ":" + std::to_string(remote.port));
        return askAndSend(remote, files, askTimeout, listener, deviceInfo);
    } catch (std::exception &e) {
        Logger::log(Logger::LEVEL_ERROR, "resolve error: " + std::string(e.what()));
        resolveThread.join();
        return false;
    }
}

namespace flowdrop {
    class SendRequest::Impl {
    public:
        Impl() = default;
        ~Impl() = default;

        [[nodiscard]] DeviceInfo getDeviceInfo() const {
            return _deviceInfo;
        }
        void setDeviceInfo(const DeviceInfo &info) {
            _deviceInfo = info;
        }

        [[nodiscard]] std::string getReceiverId() const {
            return _receiverId;
        }
        void setReceiverId(const std::string &id) {
            _receiverId = id;
        }

        [[nodiscard]] std::vector<File *> getFiles() const {
            return _files;
        }
        void setFiles(const std::vector<File *>& files) {
            _files = files;
        }

        [[nodiscard]] std::chrono::milliseconds getResolveTimeout() const {
            return _resolveTimeout;
        }
        void setResolveTimeout(const std::chrono::milliseconds &timeout) {
            _resolveTimeout = timeout;
        }

        [[nodiscard]] std::chrono::milliseconds getAskTimeout() const {
            return _askTimeout;
        }
        void setAskTimeout(const std::chrono::milliseconds &timeout) {
            _askTimeout = timeout;
        }

        [[nodiscard]] IEventListener* getEventListener() const {
            return _eventListener;
        }
        void setEventListener(IEventListener *listener) {
            _eventListener = listener;
        }

        bool execute() {
            return send(_receiverId, _files, _resolveTimeout, _askTimeout, _eventListener, _deviceInfo);
        }

    private:
        DeviceInfo _deviceInfo;
        std::string _receiverId;
        std::vector<File *> _files;
        std::chrono::milliseconds _resolveTimeout = std::chrono::milliseconds(10 * 1000); // 10 secs
        std::chrono::milliseconds _askTimeout = std::chrono::milliseconds(60 * 1000); // 60 secs
        IEventListener *_eventListener = nullptr;
    };

    SendRequest::SendRequest() : pImpl(new Impl) {}
    SendRequest::~SendRequest() = default;

    [[maybe_unused]] SendRequest& SendRequest::setDeviceInfo(const DeviceInfo& info) {
        pImpl->setDeviceInfo(info);
        return *this;
    }

    [[maybe_unused]] SendRequest& SendRequest::setReceiverId(const std::string& id) {
        pImpl->setReceiverId(id);
        return *this;
    }

    [[maybe_unused]] SendRequest& SendRequest::setFiles(const std::vector<File *>& files) {
        pImpl->setFiles(files);
        return *this;
    }

    [[maybe_unused]] SendRequest& SendRequest::setResolveTimeout(const std::chrono::milliseconds& timeout) {
        pImpl->setResolveTimeout(timeout);
        return *this;
    }

    [[maybe_unused]] SendRequest& SendRequest::setAskTimeout(const std::chrono::milliseconds& timeout) {
        pImpl->setAskTimeout(timeout);
        return *this;
    }

    [[maybe_unused]] SendRequest& SendRequest::setEventListener(IEventListener* listener) {
        pImpl->setEventListener(listener);
        return *this;
    }

    [[maybe_unused]] DeviceInfo SendRequest::getDeviceInfo() const {
        return pImpl->getDeviceInfo();
    }

    [[maybe_unused]] std::string SendRequest::getReceiverId() const {
        return pImpl->getReceiverId();
    }

    [[maybe_unused]] std::vector<File *> SendRequest::getFiles() const {
        return pImpl->getFiles();
    }

    [[maybe_unused]] std::chrono::milliseconds SendRequest::getResolveTimeout() const {
        return pImpl->getResolveTimeout();
    }

    [[maybe_unused]] std::chrono::milliseconds SendRequest::getAskTimeout() const {
        return pImpl->getAskTimeout();
    }

    [[maybe_unused]] IEventListener* SendRequest::getEventListener() const {
        return pImpl->getEventListener();
    }

    bool SendRequest::execute() {
        return pImpl->execute();
    }
}
