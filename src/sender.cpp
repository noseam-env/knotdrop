/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#include "flowdrop.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <future>
#include "specification.hpp"
#include "curl/curl.h"
#include "virtualtfa.hpp"
#include "discovery.hpp"

size_t writeCallback(char *data, size_t size, size_t nmemb, std::string *response) {
    size_t totalSize = size * nmemb;
    response->append(data, totalSize);
    return totalSize;
}

bool ask(const std::string &baseUrl, const std::vector<std::string> &files, const std::chrono::milliseconds &timeout, const flowdrop::DeviceInfo &deviceInfo) {
    std::vector<flowdrop::FileInfo> filesInfo(files.size());
    for (size_t i = 0; i < files.size(); ++i) {
        std::ifstream file(files[i], std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << files[i] << std::endl;
            return false;
        }
        std::streampos fileSize = file.tellg();
        filesInfo[i].name = files[i];
        filesInfo[i].size = static_cast<std::size_t>(fileSize);
        file.close();
    }

    flowdrop::SendAsk askData;
    askData.sender = deviceInfo;
    askData.files = filesInfo;

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
        std::cerr << "Ask error: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }
    curl_easy_cleanup(curl);

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
    auto *tfa = static_cast<VirtualTfaWriter *>(userdata);
    return tfa->writeTo(buffer, size * nmemb);
}

class SendProgressListener : public IProgressListener {
public:
    SendProgressListener(flowdrop::IEventListener *eventListener) : m_eventListener(eventListener) {}

    void totalProgress(std::size_t currentSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onSendingTotalProgress(m_totalSize, currentSize);
        }
    }

    void fileStart(char *fileName, std::size_t fileSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onSendingFileStart({fileName, fileSize});
        }
    }

    void fileProgress(char *fileName, std::size_t fileSize, std::size_t currentSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onSendingFileProgress({fileName, fileSize}, currentSize);
        }
    }

    void fileEnd(char *fileName, std::size_t fileSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onSendingFileEnd({fileName, fileSize});
        }
    }

    void setTotalSize(std::size_t totalSize) {
        SendProgressListener::m_totalSize = totalSize;
    }

private:
    flowdrop::IEventListener *m_eventListener;
    std::size_t m_totalSize = 0;
};

void sendFiles(const std::string &baseUrl, const std::vector<std::string> &files, flowdrop::IEventListener *listener, const flowdrop::DeviceInfo &deviceInfo) {
    VirtualTfaArchive *archive = virtual_tfa_archive_new();

    for (const std::string &filePath: files) {
        VirtualTfaEntry *entry = virtual_tfa_entry_new(filePath);
        virtual_tfa_archive_add(archive, entry);
    }

    auto *sendProgressListener = new SendProgressListener(listener);
    auto *tfa = new VirtualTfaWriter(archive, sendProgressListener);

    std::size_t totalSize = tfa->calcSize();
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

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "Send file error: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        curl_global_cleanup();
    }

    virtual_tfa_archive_close(archive);
}

bool askAndSend(const Address &address, const std::vector<std::string> &files, const std::chrono::milliseconds askTimeout,
                flowdrop::IEventListener *listener, const flowdrop::DeviceInfo &deviceInfo) {
    std::string baseUrl = "http://" + address.host + ":" + std::to_string(address.port) + "/";

    if (listener != nullptr) {
        listener->onAskingReceiver();
    }

    if (!ask(baseUrl, files, askTimeout, deviceInfo)) {
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

bool send(const std::string &receiverId, const std::vector<std::string> &files,
          const std::chrono::milliseconds &resolveTimeout, const std::chrono::milliseconds &askTimeout,
          flowdrop::IEventListener *listener, const flowdrop::DeviceInfo &deviceInfo) {
    if (listener != nullptr) {
        listener->onResolving();
    }

    std::promise<Address> addressPromise;
    std::future<Address> addressFuture = addressPromise.get_future();

    std::thread resolveThread([receiverId, &addressPromise]() {
        resolve(receiverId, [&addressPromise](const Address &address) {
            addressPromise.set_value(address);
        });
    });

    std::future_status status = addressFuture.wait_for(resolveTimeout);

    bool result;
    if (status == std::future_status::ready) {
        Address address = addressFuture.get();
        if (listener != nullptr) {
            listener->onResolved();
        }
        if (flowdrop::debug) {
            std::cout << "resolved: " << address.host << ":" << std::to_string(address.port) << std::endl;
        }
        result = askAndSend(address, files, askTimeout, listener, deviceInfo);
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
    SendRequest::SendRequest() {}

    SendRequest& SendRequest::setDeviceInfo(const DeviceInfo& info) {
        deviceInfo = info;
        return *this;
    }

    SendRequest& SendRequest::setReceiverId(const std::string& id) {
        receiverId = id;
        return *this;
    }

    SendRequest& SendRequest::setFiles(const std::vector<std::string>& files) {
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

    DeviceInfo SendRequest::getDeviceInfo() const {
        return deviceInfo;
    }

    std::string SendRequest::getReceiverId() const {
        return receiverId;
    }

    std::vector<std::string> SendRequest::getFiles() const {
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
