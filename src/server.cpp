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
#include "knotport/knotport.h"
#include "discovery.hpp"
#include "specification.h"
#include "virtualtfa.h"
#include "logger.h"

class ReceiveProgressListener {
public:
    ReceiveProgressListener(flowdrop::DeviceInfo sender, flowdrop::IEventListener *eventListener, tfa_size_t totalSize, std::vector<const virtual_tfa_file_info *> *receivedFiles) :
            _sender(std::move(sender)), _eventListener(eventListener), _totalSize(totalSize), _receivedFiles(receivedFiles) {}

    void totalProgress(tfa_size_t currentSize) {
        if (_eventListener != nullptr) {
            _eventListener->onReceivingTotalProgress(_sender, _totalSize, currentSize);
        }
    }

    void fileStart(const virtual_tfa_file_info *fileInfo) {
        if (_eventListener != nullptr) {
            _eventListener->onReceivingFileStart(_sender, {fileInfo->name, fileInfo->size});
        }
    }

    void fileProgress(const virtual_tfa_file_info *fileInfo, tfa_size_t currentSize) {
        if (_eventListener != nullptr) {
            _eventListener->onReceivingFileProgress(_sender, {fileInfo->name, fileInfo->size}, currentSize);
        }
    }

    void fileEnd(const virtual_tfa_file_info *fileInfo) {
        virtual_tfa_file_info *copiedFileInfo = (virtual_tfa_file_info *)malloc(sizeof(virtual_tfa_file_info));
        if (copiedFileInfo) {
            char *copied_name = (char *)malloc(strlen(fileInfo->name) + 1);
            strcpy(copied_name, fileInfo->name);
            copiedFileInfo->name = copied_name;
            copiedFileInfo->size = fileInfo->size;
            copiedFileInfo->ctime = fileInfo->ctime;
            copiedFileInfo->mtime = fileInfo->mtime;
            copiedFileInfo->mode = fileInfo->mode;
            _receivedFiles->push_back(copiedFileInfo);
        }
        if (_eventListener != nullptr) {
            _eventListener->onReceivingFileEnd(_sender, {fileInfo->name, fileInfo->size});
        }
    }

private:
    flowdrop::DeviceInfo _sender;
    flowdrop::IEventListener *_eventListener;
    tfa_size_t _totalSize;
    std::vector<const virtual_tfa_file_info *> *_receivedFiles;
};

namespace server_listener {
    void total_progress(void *userdata, tfa_size_t currentSize) {
        auto *listener = static_cast<ReceiveProgressListener *>(userdata);
        listener->totalProgress(currentSize);
    }

    void file_start(void *userdata, const virtual_tfa_file_info *fileInfo) {
        auto *listener = static_cast<ReceiveProgressListener *>(userdata);
        listener->fileStart(fileInfo);
    }

    void file_progress(void *userdata, const virtual_tfa_file_info *fileInfo, tfa_size_t currentSize) {
        auto *listener = static_cast<ReceiveProgressListener *>(userdata);
        listener->fileProgress(fileInfo, currentSize);
    }

    void file_end(void *userdata, const virtual_tfa_file_info *fileInfo) {
        auto *listener = static_cast<ReceiveProgressListener *>(userdata);
        listener->fileEnd(fileInfo);
    }
}

struct ReceiveSession {
    flowdrop::DeviceInfo sender{};
    virtual_tfa_reader *tfa_reader = nullptr;
    tfa_size_t totalSize = 0;
    std::vector<const virtual_tfa_file_info *> *receivedFiles = nullptr;
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
                        Logger::log(Logger::LEVEL_ERROR, "Destination path is not directory");
                        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
                    }
                    auto *receivedFiles = new std::vector<const virtual_tfa_file_info *>;
                    virtual_tfa_reader *tfa_reader = virtual_tfa_reader_new();
                    if (!tfa_reader) {
                        Logger::log(Logger::LEVEL_ERROR, "Failed to initialize virtual_tfa_reader");
                        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
                    }

                    std::string destStr = _destDir.u8string();
                    const char* dest = destStr.c_str();

                    char* destBuffer = new char[1024];
                    strcpy(destBuffer, dest);

                    virtual_tfa_reader_set_dest(tfa_reader, destBuffer);

                    auto *cppListener = static_cast<void *>(new ReceiveProgressListener(sender, _listener, totalSize, receivedFiles));
                    auto *tfaListener = new virtual_tfa_listener{
                            server_listener::total_progress,
                            cppListener,
                            server_listener::file_start,
                            cppListener,
                            server_listener::file_progress,
                            cppListener,
                            server_listener::file_end,
                            cppListener
                    };
                    virtual_tfa_reader_set_listener(tfa_reader, tfaListener);

                    session = new ReceiveSession{
                            sender,
                            tfa_reader,
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
                        if (session->tfa_reader) {
                            tfa_size_t bytes_read = 0;
                            int result = virtual_tfa_reader_read(session->tfa_reader, const_cast<char *>(data), size, &bytes_read);
                            if (result != 0 || bytes_read != size) {
                                Logger::log(Logger::LEVEL_ERROR, "Reading error, code: " + std::to_string(result));
                                ctx->close();
                                return HTTP_STATUS_INTERNAL_SERVER_ERROR;
                            }
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
                            for (const virtual_tfa_file_info *fileInfo: *session->receivedFiles) {
                                receivedFiles.push_back({fileInfo->name, fileInfo->size});
                                // TODO: clean memory from files info
                                // free((void *) fileInfo);
                            }
                            _listener->onReceivingEnd(session->sender, session->totalSize, receivedFiles);
                        }

                        if (session->tfa_reader) {
                            virtual_tfa_reader_free(session->tfa_reader);
                            session->tfa_reader = nullptr;
                        }
                        delete session;
                        ctx->userdata = nullptr;
                    }
                }
                    break;
                case HP_ERROR: {
                    if (session) {
                        if (session->tfa_reader) {
                            virtual_tfa_reader_free(session->tfa_reader);
                            session->tfa_reader = nullptr;
                        }
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
            port_t port = knotport_find_open();
            if (port == 0) {
                throw new std::runtime_error("unable to find free port");
            }
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
#if defined(IPV6_NOT_SUPPORTED)
                bool useIPv4 = true;
#else
                bool useIPv4 = false;
#endif
                discovery::announce(id, port, useIPv4, [this](){
                    return _sdStop->load();
                });
            });
            _sdThread.detach();

            _server = hv::HttpServer(&router);
#if defined(IPV6_NOT_SUPPORTED)
            _server.setHost("0.0.0.0");
#else
            _server.setHost("::");
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
