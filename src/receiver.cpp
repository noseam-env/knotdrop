/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#include "flowdrop.hpp"
#include "hv/HttpServer.h"
#include "hv/hlog.h"
#include <thread>
#include "portroller.hpp"
#include "discovery.hpp"
#include "specification.hpp"
#include "virtualtfa.hpp"
#include <set>
#include <utility>

namespace flowdrop {
    class ReceiverImpl {
    public:
        DeviceInfo deviceInfo;
        askCallback askCallback;
        std::filesystem::path destDir;
        IEventListener *listener = nullptr;
        std::thread sdThread;
        std::atomic<bool> *sdStop = nullptr;
        hv::HttpServer server;

        void stop() {
            server.stop();
            *sdStop = false;
            if (sdThread.joinable()) {
                sdThread.join();
            }
        }
    };
}

class ReceiveProgressListener : public IProgressListener {
public:
    ReceiveProgressListener(flowdrop::DeviceInfo sender, flowdrop::IEventListener *eventListener) :
            m_sender(std::move(sender)), m_eventListener(eventListener) {}

    void totalProgress(std::size_t currentSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onReceivingTotalProgress(m_sender, 0, currentSize);
        }
    }

    void fileStart(char *fileName, std::size_t fileSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onReceivingFileStart(m_sender, {fileName, fileSize});
        }
    }

    void fileProgress(char *fileName, std::size_t fileSize, std::size_t currentSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onReceivingFileProgress(m_sender, {fileName, fileSize}, currentSize);
        }
    }

    void fileEnd(char *fileName, std::size_t fileSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onReceivingFileEnd(m_sender, {fileName, fileSize});
        }
    }

private:
    flowdrop::DeviceInfo m_sender;
    flowdrop::IEventListener *m_eventListener;
};

struct ReceiveSession {
    flowdrop::DeviceInfo sender;
    VirtualTfaReader *tfa;
    std::size_t totalSize;
};

void askHandler(const HttpRequestPtr &req, const HttpResponseWriterPtr &writer, flowdrop::ReceiverImpl *receiver) {
    if (flowdrop::debug) {
        std::cout << "ask_new: " << req->Host() << std::endl;
    }

    json j;
    try {
        j = json::parse(req->Body());
    } catch (const std::exception &) {
        if (flowdrop::debug) {
            std::cout << "ask_invalid_json: " << req->Host() << std::endl;
        }
        writer->Begin();
        writer->WriteStatus(HTTP_STATUS_BAD_REQUEST);
        writer->WriteBody("Invalid JSON");
        writer->End();
        return;
    }
    flowdrop::SendAsk sendAsk = j;

    if (receiver->listener != nullptr) {
        receiver->listener->onSenderAsk(sendAsk.sender);
    }

    bool accepted = receiver->askCallback == nullptr || receiver->askCallback(sendAsk);

    if (flowdrop::debug) {
        std::cout << "ask_accepted: " << req->Host() << std::endl;
    }

    nlohmann::json resp;
    resp["accepted"] = accepted;
    std::string respString = resp.dump();

    writer->Begin();
    writer->WriteStatus(HTTP_STATUS_OK);
    writer->WriteHeader("Content-Type", APPLICATION_JSON);
    writer->WriteBody(respString);
    writer->End();
}

int sendHandler(const HttpContextPtr &ctx, http_parser_state state, const char *data, size_t size,
                flowdrop::ReceiverImpl *receiver) {
    int status_code = HTTP_STATUS_UNFINISHED;
    auto *session = (ReceiveSession *) ctx->userdata;
    switch (state) {
        case HP_HEADERS_COMPLETE: {
            if (ctx->is(MULTIPART_FORM_DATA)) {
                ctx->close();
                return HTTP_STATUS_BAD_REQUEST;
            }
            auto headers = ctx->request.get()->headers;
            auto it = headers.find(flowdrop_deviceinfo_header);
            if (it == headers.end()) {
                ctx->close();
                return HTTP_STATUS_BAD_REQUEST;
            }
            std::string senderStr = it->second;
            it = headers.find("Content-Length");
            if (it == headers.end()) {
                ctx->close();
                return HTTP_STATUS_BAD_REQUEST;
            }
            std::size_t totalSize;
            flowdrop::DeviceInfo sender;
            try {
                totalSize = std::stoull(it->second);

                json jsonData = json::parse(senderStr);
                flowdrop::from_json(jsonData, sender);
            } catch (std::exception &) {
                ctx->close();
                return HTTP_STATUS_BAD_REQUEST;
            }
            std::vector<flowdrop::FileInfo> receivedFiles;
            std::filesystem::file_status destStatus = status(receiver->destDir);
            if (!exists(destStatus)) {
                create_directories(receiver->destDir);
            } else if (!is_directory(destStatus)) {
                std::cerr << "Destination path is not directory" << std::endl;
                return HTTP_STATUS_INTERNAL_SERVER_ERROR;
            }
            auto tfa = new VirtualTfaReader(receiver->destDir, new ReceiveProgressListener(sender, receiver->listener));
            session = new ReceiveSession{
                    sender,
                    tfa,
                    totalSize
            };
            ctx->userdata = session;
            if (receiver->listener != nullptr) {
                receiver->listener->onReceivingStart(sender, totalSize);
            }
        }
            break;
        case HP_BODY: {
            if (session && data && size) {
                if (session->tfa->addReadData(const_cast<char *>(data), size) != size) {
                    ctx->close();
                    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
                }
            }
        }
            break;
        case HP_MESSAGE_COMPLETE: {
            status_code = HTTP_STATUS_OK;
            HttpResponse *resp = ctx->response.get();
            resp->Set("code", HTTP_STATUS_OK);
            resp->Set("message", http_status_str(HTTP_STATUS_OK));
            ctx->send();
            if (session) {
                if (receiver->listener != nullptr) {
                    receiver->listener->onReceivingEnd(session->sender, session->totalSize);
                }

                delete session->tfa;
                delete session;
                ctx->userdata = nullptr;
            }
        }
            break;
        case HP_ERROR: {
            if (session) {
                delete session->tfa;
                delete session;
                ctx->userdata = nullptr;
            }
        }
            break;
        default:
            break;
    }
    return status_code;
}

void receive(flowdrop::ReceiverImpl *receiver, bool wait) {
    unsigned short port = rollAvailablePort();
    if (flowdrop::debug) {
        std::cout << "port: " << port << std::endl;
    }

    std::string slash = "/";
    std::string deviceInfoStr = json(receiver->deviceInfo).dump();

    hlog_set_level(LOG_LEVEL_SILENT);

    HttpService router;
    router.GET((slash + flowdrop_endpoint_device_info).c_str(),
               [&deviceInfoStr](HttpRequest *req, HttpResponse *resp) {
                   resp->SetContentType(APPLICATION_JSON);
                   return resp->String(deviceInfoStr);
               });
    router.POST((slash + flowdrop_endpoint_ask).c_str(),
                [receiver](const HttpRequestPtr &req, const HttpResponseWriterPtr &writer) {
                    askHandler(req, writer, receiver);
                });
    router.POST((slash + flowdrop_endpoint_send).c_str(),
                [receiver](const HttpContextPtr &ctx, http_parser_state state, const char *data,
                           size_t size) {
                    return sendHandler(ctx, state, data, size, receiver);
                });

    std::string id = receiver->deviceInfo.id;
    std::atomic<bool> stopFlag(false);
    receiver->sdStop = &stopFlag;
    receiver->sdThread = std::thread([&id, &port, &stopFlag]() {
        announce(id, port, stopFlag);
    });
    receiver->sdThread.detach();

    receiver->server = hv::HttpServer(&router);
    receiver->server.setPort(port);
    receiver->server.setThreadNum(3);
    bool started = false;
    receiver->server.onWorkerStart = [receiver, &port, &started]() {
        if (started) return;
        started = true;
        if (receiver->listener != nullptr) {
            receiver->listener->onReceiverStarted(port);
        }
    };
    receiver->server.run(wait);
    /*if (listener != nullptr) {
        listener->onReceiverStarted(port);
    }*/
}

namespace flowdrop {
    Receiver::Receiver(const DeviceInfo &deviceInfo) {
        pImpl = new ReceiverImpl();
        pImpl->deviceInfo = deviceInfo;
    }

    Receiver::~Receiver() {
        delete pImpl;
    }

    const DeviceInfo &Receiver::getDeviceInfo() const {
        return pImpl->deviceInfo;
    }

    void Receiver::setDestDir(const std::filesystem::path &destDir) {
        pImpl->destDir = destDir;
    }

    const std::filesystem::path &Receiver::getDestDir() const {
        return pImpl->destDir;
    }

    void Receiver::setAskCallback(const askCallback &askCallback) {
        pImpl->askCallback = askCallback;
    }

    const askCallback &Receiver::getAskCallback() const {
        return pImpl->askCallback;
    }

    void Receiver::setEventListener(IEventListener *listener) {
        pImpl->listener = listener;
    }

    IEventListener *Receiver::getEventListener() {
        return pImpl->listener;
    }

    void Receiver::run(bool wait) {
        receive(pImpl, wait);
    }

    void Receiver::stop() {
        pImpl->stop();
    }
}
