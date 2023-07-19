/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
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
#include <atomic>

class ReceiveProgressListener : public IProgressListener {
public:
    ReceiveProgressListener(flowdrop::DeviceInfo sender, flowdrop::IEventListener *eventListener, std::size_t totalSize) :
            m_sender(std::move(sender)), m_eventListener(eventListener), m_totalSize(totalSize) {}

    void totalProgress(std::size_t currentSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onReceivingTotalProgress(m_sender, m_totalSize, currentSize);
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
    std::size_t m_totalSize;
};

struct ReceiveSession {
    flowdrop::DeviceInfo sender{};
    VirtualTfaReader *tfa = nullptr;
    std::size_t totalSize{};
};

namespace flowdrop {
    struct Receiver::Impl {
        DeviceInfo _deviceInfo;
        askCallback _askCallback;
        std::filesystem::path _destDir;
        IEventListener *_listener = nullptr;
        std::thread _sdThread;
        std::atomic<bool> *_sdStop = nullptr;
        hv::HttpServer _server;

        void askHandler(const HttpRequestPtr &req, const HttpResponseWriterPtr &writer) {
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

            if (_listener != nullptr) {
                _listener->onSenderAsk(sendAsk.sender);
            }

            bool accepted = _askCallback == nullptr || _askCallback(sendAsk);

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

        int sendHandler(const HttpContextPtr &ctx, http_parser_state state, const char *data, size_t size) {
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
                    flowdrop::DeviceInfo sender;
                    std::size_t totalSize;
                    try {
                        json jsonData = json::parse(senderStr);
                        flowdrop::from_json(jsonData, sender);

                        totalSize = std::stoull(it->second);
                    } catch (std::exception &) {
                        ctx->close();
                        return HTTP_STATUS_BAD_REQUEST;
                    }
                    std::vector<flowdrop::FileInfo> receivedFiles;
                    std::filesystem::file_status destStatus = status(_destDir);
                    if (!exists(destStatus)) {
                        create_directories(_destDir);
                    } else if (!is_directory(destStatus)) {
                        std::cerr << "Destination path is not directory" << std::endl;
                        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
                    }
                    auto tfa = new VirtualTfaReader(_destDir, new ReceiveProgressListener(sender, _listener, totalSize));
                    session = new ReceiveSession{
                            sender,
                            tfa,
                            totalSize
                    };
                    ctx->userdata = session;
                    if (_listener != nullptr) {
                        _listener->onReceivingStart(sender, totalSize);
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
                        if (_listener != nullptr) {
                            _listener->onReceivingEnd(session->sender, session->totalSize);
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

        void receive(bool wait) {
            unsigned short port = rollAvailablePort(flowdrop_default_port);
            if (flowdrop::debug) {
                std::cout << "port: " << port << std::endl;
            }

            std::string slash = "/";
            std::string deviceInfoStr = json(_deviceInfo).dump();

            hlog_set_level(LOG_LEVEL_SILENT);

            HttpService router;
            router.GET((slash + flowdrop_endpoint_device_info).c_str(),
                       [&deviceInfoStr](HttpRequest *req, HttpResponse *resp) {
                           resp->SetContentType(APPLICATION_JSON);
                           return resp->String(deviceInfoStr);
                       });
            router.POST((slash + flowdrop_endpoint_ask).c_str(),
                        [this](const HttpRequestPtr &req, const HttpResponseWriterPtr &writer) {
                            askHandler(req, writer);
                        });
            router.POST((slash + flowdrop_endpoint_send).c_str(),
                        [this](const HttpContextPtr &ctx, http_parser_state state, const char *data,
                                   size_t size) {
                            return sendHandler(ctx, state, data, size);
                        });

            std::string id = _deviceInfo.id;
            _sdStop = new std::atomic<bool>(false);
            _sdThread = std::thread([&id, &port, this]() {
                announce(id, port, *_sdStop);
            });
            _sdThread.detach();

            _server = hv::HttpServer(&router);
            _server.setPort(port);
            _server.setThreadNum(3);
            bool started = false;
            _server.onWorkerStart = [this, &port, &started]() {
                if (started) return;
                started = true;
                if (_listener != nullptr) {
                    _listener->onReceiverStarted(port);
                }
            };
            _server.run(wait);
            /*if (listener != nullptr) {
                listener->onReceiverStarted(port);
            }*/
        }

        void stop() {
            _server.stop();
            *_sdStop = true;
            if (_sdThread.joinable()) {
                _sdThread.join();
            }
        }
    };

    Receiver::Receiver(const DeviceInfo &deviceInfo) : pImpl(new Impl) {
        pImpl->_deviceInfo = deviceInfo;
    }

    Receiver::~Receiver() = default;

    const DeviceInfo &Receiver::getDeviceInfo() const {
        return pImpl->_deviceInfo;
    }

    void Receiver::setDestDir(const std::filesystem::path &destDir) {
        pImpl->_destDir = destDir;
    }

    const std::filesystem::path &Receiver::getDestDir() const {
        return pImpl->_destDir;
    }

    void Receiver::setAskCallback(const askCallback &askCallback) {
        pImpl->_askCallback = askCallback;
    }

    const askCallback &Receiver::getAskCallback() const {
        return pImpl->_askCallback;
    }

    void Receiver::setEventListener(IEventListener *listener) {
        pImpl->_listener = listener;
    }

    IEventListener *Receiver::getEventListener() {
        return pImpl->_listener;
    }

    void Receiver::run(bool wait) {
        pImpl->receive(wait);
    }

    void Receiver::stop() {
        pImpl->stop();
    }
} // namespace flowdrop
