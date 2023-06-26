/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#include <iostream>
#include "flowdrop.h"
#include "flowdrop.hpp"

namespace util {
    flowdrop::DeviceInfo from_c(DeviceInfo *deviceInfo) {
        return {
                deviceInfo->id,
                deviceInfo->uuid,
                deviceInfo->name,
                deviceInfo->model,
                deviceInfo->platform,
                deviceInfo->system_version
        };
    }

    DeviceInfo to_c(const flowdrop::DeviceInfo &info) {
        return {
                const_cast<char *>(info.id.c_str()),
                const_cast<char *>(info.uuid.has_value() ? nullptr : info.uuid.value().c_str()),
                const_cast<char *>(info.name.has_value() ? nullptr : info.name.value().c_str()),
                const_cast<char *>(info.model.has_value() ? nullptr : info.model.value().c_str()),
                const_cast<char *>(info.platform.has_value() ? nullptr : info.platform.value().c_str()),
                const_cast<char *>(info.system_version.has_value() ? nullptr : info.system_version.value().c_str())
        };
    }

    FileInfo to_c(const flowdrop::FileInfo &fileInfo) {
        return {const_cast<char *>(fileInfo.name.c_str()), fileInfo.size};
    }

    SendAsk to_c(const flowdrop::SendAsk &sendAsk) {
        SendAsk ask = {};
        ask.sender = to_c(sendAsk.sender);
        ask.files = new FileInfo[sendAsk.files.size()];
        ask.file_count = sendAsk.files.size();
        for (size_t i = 0; i < sendAsk.files.size(); ++i) {
            ask.files[i] = to_c(sendAsk.files[i]);
        }
        return ask;
    }
}

// C

char *generate_md5_id() {
    std::string result = flowdrop::generate_md5_id();
    char *cstr = new char[result.length() + 1];
    strcpy(cstr, result.c_str());
    return cstr;
}

void find(findCallback callback) {
    flowdrop::find([&callback](const flowdrop::DeviceInfo &device) {
        const DeviceInfo cdevice = util::to_c(device);
        callback(&cdevice);
    });
}

struct SendRequest {
    flowdrop::SendRequest *pImpl;
};

SendRequest *SendRequest_SendRequest() {
    auto *sendRequest = new flowdrop::SendRequest();
    auto *c_sendRequest = new SendRequest{sendRequest};
    return c_sendRequest;
}

bool SendRequest_execute(SendRequest *request) {
    return request->pImpl->execute();
}

struct Receiver {
    flowdrop::Receiver *pImpl;
    askCallback callback;
    IEventListener *listener;
};

Receiver *Receiver_Receiver(DeviceInfo *deviceInfo) {
    DeviceInfo deviceInfoCopy{};
    memcpy(&deviceInfoCopy, deviceInfo, sizeof(DeviceInfo));
    auto *receiver = new flowdrop::Receiver(util::from_c(&deviceInfoCopy));
    return new Receiver{receiver};
}

const DeviceInfo *Receiver_getDeviceInfo(const Receiver *receiver) {
    const flowdrop::DeviceInfo &deviceInfo = receiver->pImpl->getDeviceInfo();
    DeviceInfo c_deviceInfo = util::to_c(deviceInfo);
    return &c_deviceInfo;
}

const char *Receiver_getDestDir(const Receiver *receiver) {
    std::string result = receiver->pImpl->getDestDir().string();
    char *cstr = new char[result.length() + 1];
    strcpy(cstr, result.c_str());
    return cstr;
}

void Receiver_setDestDir(Receiver *receiver, const char *destDir) {
    receiver->pImpl->setDestDir(destDir);
}

askCallback Receiver_getAskCallback(const Receiver *receiver) {
    return receiver->callback;
}

void Receiver_setAskCallback(Receiver *receiver, askCallback callback) {
    receiver->callback = callback;
    receiver->pImpl->setAskCallback([&callback](const flowdrop::SendAsk &sendAsk) {
        SendAsk c_sendAsk = util::to_c(sendAsk);
        return callback(&c_sendAsk);
    });
}

IEventListener *Receiver_getEventListener(const Receiver *receiver) {
    return nullptr;
}

void Receiver_setEventListener(Receiver *receiver, IEventListener *listener) {

}

void Receiver_run(Receiver *receiver, bool wait) {
    receiver->pImpl->run(wait);
}

void Receiver_stop(Receiver *receiver) {
    receiver->pImpl->stop();
}
