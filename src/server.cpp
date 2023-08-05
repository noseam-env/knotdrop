/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "flowdrop/flowdrop.hpp"
#include "core.h"
#include "hv/HttpServer.h"
#include "hv/hlog.h"
#include <thread>
#include <set>
#include <utility>
#include <atomic>
#include "portroller.hpp"
#include "discovery.hpp"
#include "specification.h"
#include "virtualtfa.hpp"
#include "logger.h"

class ReceiveProgressListener : public IProgressListener {
public:
    ReceiveProgressListener(flowdrop::DeviceInfo sender, flowdrop::IEventListener *eventListener, std::uint64_t totalSize, std::vector<FileInfo> *receivedFiles) :
            m_sender(std::move(sender)), m_eventListener(eventListener), m_totalSize(totalSize), m_receivedFiles(receivedFiles) {}

    void totalProgress(std::uint64_t currentSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onReceivingTotalProgress(m_sender, m_totalSize, currentSize);
        }
    }

    void fileStart(const FileInfo &fileInfo) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onReceivingFileStart(m_sender, {fileInfo.name, fileInfo.size});
        }
    }

    void fileProgress(const FileInfo &fileInfo, std::uint64_t currentSize) override {
        if (m_eventListener != nullptr) {
            m_eventListener->onReceivingFileProgress(m_sender, {fileInfo.name, fileInfo.size}, currentSize);
        }
    }

    void fileEnd(const FileInfo &fileInfo) override {
        m_receivedFiles->push_back(fileInfo);
        if (m_eventListener != nullptr) {
            m_eventListener->onReceivingFileEnd(m_sender, {fileInfo.name, fileInfo.size});
        }
    }

private:
    flowdrop::DeviceInfo m_sender;
    flowdrop::IEventListener *m_eventListener;
    std::uint64_t m_totalSize;
    std::vector<FileInfo> *m_receivedFiles;
};

struct ReceiveSession {
    flowdrop::DeviceInfo sender{};
    VirtualTfaReader *tfa = nullptr;
    std::uint64_t totalSize{};
    std::vector<FileInfo> *receivedFiles = nullptr;
};

namespace flowdrop {
    class Server::Impl {
    public:
        Impl() = default;
        ~Impl() = default;

        DeviceInfo _deviceInfo;
        askCallback _askCallback;
        std::filesystem::path _destDir;
        IEventListener *_listener = nullptr;
        std::thread _sdThread;
        std::atomic<bool> *_sdStop = nullptr;
        hv::HttpServer _server;
        //std::unordered_map<std::string, CachedSendRequest> _sendKeys;

        void askHandler(const HttpRequestPtr &req, const HttpResponseWriterPtr &writer) {
            std::string senderIp = req->client_addr.ip;
            Logger::log(Logger::LEVEL_DEBUG, "ask_new: " + senderIp);

            json j;
            try {
                j = json::parse(req->Body());
            } catch (const std::exception &) {
                Logger::log(Logger::LEVEL_DEBUG, "ask_invalid_json: " + senderIp);
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

            /*std::string sendKey = flowdrop::generate_md5_id();
            _sendKeys.insert({sendKey, {senderIp, sendAsk.sender}});
            std::thread th([this, &sendKey](){
                std::this_thread::sleep_for(std::chrono::minutes(1));
            });*/

            Logger::log(Logger::LEVEL_DEBUG, "ask_accepted: " + senderIp);

            nlohmann::json resp;
            resp["accepted"] = accepted;
            //resp["key"] = sendKey;
            std::string respString = resp.dump();

            writer->Begin();
            writer->WriteStatus(HTTP_STATUS_OK);
            writer->WriteHeader("Content-Type", APPLICATION_JSON);
            writer->WriteBody(respString);
            writer->End();
        }

        int sendHandler(const HttpContextPtr &ctx, http_parser_state state, const char *data, size_t size) {
            //std::string senderIp = ctx->ip();
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
                    std::uint64_t totalSize;
                    try {
                        json jsonData = json::parse(senderStr);
                        flowdrop::from_json(jsonData, sender);

                        totalSize = std::stoull(it->second);
                    } catch (std::exception &) {
                        ctx->close();
                        return HTTP_STATUS_BAD_REQUEST;
                    }
                    std::filesystem::file_status destStatus = status(_destDir);
                    if (!exists(destStatus)) {
                        create_directories(_destDir);
                    } else if (!is_directory(destStatus)) {
                        std::cerr << "Destination path is not directory" << std::endl;
                        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
                    }
                    auto *receivedFiles = new std::vector<::FileInfo>;
                    auto tfa = new VirtualTfaReader(_destDir, new ReceiveProgressListener(sender, _listener, totalSize, receivedFiles));
                    session = new ReceiveSession{
                            sender,
                            tfa,
                            totalSize,
                            receivedFiles
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
                            std::vector<FileInfo> receivedFiles;
                            for (const ::FileInfo& fileInfo: *session->receivedFiles) {
                                receivedFiles.push_back({fileInfo.name, fileInfo.size});
                            }
                            _listener->onReceivingEnd(session->sender, session->totalSize, receivedFiles);
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

        void run() {
            unsigned short port = rollAvailablePort(flowdrop_default_port);
            Logger::log(Logger::LEVEL_DEBUG, "server port: " + std::to_string(port));

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
#if !defined(ANDROID)
                bool useIPv4 = false;
#else
                bool useIPv4 = true;
#endif
                discovery::announce(id, port, useIPv4, [this](){
                    return _sdStop->load();
                });
            });
            _sdThread.detach();

            _server = hv::HttpServer(&router);
#if !defined(ANDROID)
            _server.setHost("::");
#else
            _server.setHost("0.0.0.0");
#endif
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
            //int sockfd = Bind(port, "::", SOCK_STREAM);
            //sockfd = ListenFD(sockfd);
            //_server.setListenFD(sockfd, 0);
            _server.run(true);
        }

        void stop() {
            _server.stop();
            *_sdStop = true;
            if (_sdThread.joinable()) {
                _sdThread.join();
            }
        }
    };

    Server::Server(const DeviceInfo &deviceInfo) : pImpl(new Impl) {
        pImpl->_deviceInfo = deviceInfo;
    }

    Server::~Server() = default;

    [[maybe_unused]] const DeviceInfo &Server::getDeviceInfo() const {
        return pImpl->_deviceInfo;
    }

    void Server::setDestDir(const std::filesystem::path &destDir) {
        pImpl->_destDir = destDir;
    }

    [[maybe_unused]] const std::filesystem::path &Server::getDestDir() const {
        return pImpl->_destDir;
    }

    void Server::setAskCallback(const askCallback &askCallback) {
        pImpl->_askCallback = askCallback;
    }

    [[maybe_unused]] const askCallback &Server::getAskCallback() const {
        return pImpl->_askCallback;
    }

    void Server::setEventListener(IEventListener *listener) {
        pImpl->_listener = listener;
    }

    [[maybe_unused]] IEventListener *Server::getEventListener() {
        return pImpl->_listener;
    }

    void Server::run() {
        pImpl->run();
    }

    void Server::stop() {
        pImpl->stop();
    }
} // namespace flowdrop
