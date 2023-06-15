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

static int response_status(HttpResponse *resp, int code = 200, const char *message = nullptr) {
    if (message == nullptr) message = http_status_str((enum http_status) code);
    resp->Set("code", code);
    resp->Set("message", message);
    return code;
}

static int response_status(const HttpResponseWriterPtr &writer, int code = 200, const char *message = nullptr) {
    response_status(writer->response.get(), code, message);
    writer->End();
    return code;
}

static int response_status(const HttpContextPtr &ctx, int code = 200, const char *message = nullptr) {
    response_status(ctx->response.get(), code, message);
    ctx->send();
    return code;
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

int sendHandler(const HttpContextPtr &ctx, http_parser_state state, const char *data, size_t size,
                const std::string &dest, flowdrop::IEventListener *listener) {
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
            auto tfa = new VirtualTfaReader(dest, new ReceiveProgressListener(sender, listener));
            session = new ReceiveSession{
                    sender,
                    tfa,
                    totalSize
            };
            ctx->userdata = session;
            if (listener != nullptr) {
                listener->onReceivingStart(sender, totalSize);
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
            ctx->setContentType(APPLICATION_JSON);
            response_status(ctx, status_code);
            if (session) {
                if (listener != nullptr) {
                    listener->onReceivingEnd(session->sender, session->totalSize);
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

void flowdrop::receive(const std::string &dest, const sendAskCallback &callback, IEventListener *listener) {
    hlog_set_level(LOG_LEVEL_SILENT);

    HttpService router;

    std::string slash = "/";

    router.GET((slash + flowdrop_endpoint_device_info).c_str(), [](HttpRequest *req, HttpResponse *resp) {
        resp->SetContentType(APPLICATION_JSON);
        json j = flowdrop::thisDeviceInfo;
        return resp->String(j.dump());
    });

    router.POST((slash + flowdrop_endpoint_ask).c_str(),
                [&callback, listener](const HttpRequestPtr &req, const HttpResponseWriterPtr &writer) {
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

                    if (listener != nullptr) {
                        listener->onSenderAsk(sendAsk.sender);
                    }

                    bool accepted = callback(sendAsk);

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
                });

    router.POST((slash + flowdrop_endpoint_send).c_str(),
                [dest, listener](const HttpContextPtr &ctx, http_parser_state state, const char *data, size_t size) {
                    return sendHandler(ctx, state, data, size, dest, listener);
                });

    unsigned short port = rollAvailablePort();

    if (flowdrop::debug) {
        std::cout << "port: " << port << std::endl;
    }

    std::thread sdThread([&port]() {
        announce(port);
    });
    sdThread.detach();

    if (listener != nullptr) {
        listener->onReceiverStarted(port);
    }

    hv::HttpServer server(&router);
    server.setPort(port);
    server.setThreadNum(4);
    server.run();
}
