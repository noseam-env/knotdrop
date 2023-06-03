/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#include "../include/flowdrop.hpp"
#include "hv/HttpServer.h"
#include "hv/hlog.h"
#include <thread>
#include "portroller.hpp"
#include "discovery.hpp"
#include "libarchive.h"
#include "pipe.hpp"
#include "specification.hpp"
#include <set>

static int response_status(HttpResponse *resp, int code = 200, const char *message = NULL) {
    if (message == NULL) message = http_status_str((enum http_status) code);
    resp->Set("code", code);
    resp->Set("message", message);
    return code;
}

static int response_status(const HttpResponseWriterPtr &writer, int code = 200, const char *message = NULL) {
    response_status(writer->response.get(), code, message);
    writer->End();
    return code;
}

static int response_status(const HttpContextPtr &ctx, int code = 200, const char *message = NULL) {
    response_status(ctx->response.get(), code, message);
    ctx->send();
    return code;
}

void untar(FILE *file, const std::string &dest) {
    std::filesystem::path destPath = dest;
    std::fseek(file, 0, SEEK_SET);

    struct archive *a = archive_read_new();
    archive_read_support_format_tar(a);
    archive_read_open_FILE(a, file);

    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string filename = archive_entry_pathname(entry);
        std::filesystem::path targetPath = destPath / filename;
        archive_entry_set_pathname(entry, targetPath.string().c_str());

        archive_read_extract(a, entry, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_ACL);

        std::cout << "Accepted " << filename << std::endl;
    }

    archive_read_close(a);
    archive_read_free(a);
}

int sendHandler(const HttpContextPtr &ctx, http_parser_state state, const char *data, size_t size,
                const std::string &dest) {
    // printf("recvLargeFile state=%d\n", (int)state);
    int status_code = HTTP_STATUS_UNFINISHED;
    HFile *file = (HFile *) ctx->userdata;
    switch (state) {
        case HP_HEADERS_COMPLETE: {
            if (ctx->is(MULTIPART_FORM_DATA)) {
                // NOTE: You can use multipart_parser if you want to use multipart/form-data.
                ctx->close();
                return HTTP_STATUS_BAD_REQUEST;
            }
            std::cout << "Accepting files ..." << std::endl;
            file = new HFile;
            file->fp = std::tmpfile();
            /*if (file->open(filepath.c_str(), "wb") != 0) {
                ctx->close();
                return HTTP_STATUS_INTERNAL_SERVER_ERROR;
            }*/
            ctx->userdata = file;
        }
            break;
        case HP_BODY: {
            if (file && data && size) {
                if (file->write(data, size) != size) {
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
            if (file) {
                untar(file->fp, dest);
                delete file;
                ctx->userdata = NULL;
            }
        }
            break;
        case HP_ERROR: {
            if (file) {
                file->remove();
                delete file;
                ctx->userdata = NULL;
            }
        }
            break;
        default:
            break;
    }
    return status_code;
}

void flowdrop::receive(const std::string &dest, const sendAskCallback &callback) {
    hlog_set_level(LOG_LEVEL_SILENT);

    HttpService router;

    std::string slash = "/";

    router.GET((slash + flowdrop_endpoint_device_info).c_str(), [](HttpRequest *req, HttpResponse *resp) {
        resp->SetContentType(APPLICATION_JSON);
        json j = flowdrop::thisDeviceInfo;
        return resp->String(j.dump());
    });

    router.POST((slash + flowdrop_endpoint_ask).c_str(), [&callback](HttpRequest *req, HttpResponse *resp) {
        if (flowdrop::debug) {
            std::cout << "ask_new: " << req->Host() << std::endl;
        }

        json j;
        try {
            j = json::parse(req->Body());
        } catch (const std::exception &ex) {
            if (flowdrop::debug) {
                std::cout << "ask_invalid_json: " << req->Host() << std::endl;
            }
            resp->status_code = HTTP_STATUS_BAD_REQUEST;
            return resp->String("Invalid JSON");
        }

        flowdrop::SendAsk sendAsk{};

        bool accepted = callback(sendAsk);

        if (flowdrop::debug) {
            std::cout << "ask_accepted: " << req->Host() << std::endl;
        }

        resp->json["accepted"] = accepted;
        return 200;
    });

    router.POST((slash + flowdrop_endpoint_send).c_str(),
                [dest](const HttpContextPtr &ctx, http_parser_state state, const char *data, size_t size) {
                    return sendHandler(ctx, state, data, size, dest);
                });

    unsigned short port = rollAvailablePort();

    if (flowdrop::debug) {
        std::cout << "port: " << port << std::endl;
    }

    std::thread sdThread([&port]() {
        announce(port);
    });
    sdThread.detach();

    std::cout << "Receiving as " << flowdrop::thisDeviceInfo.id << std::endl;
    std::cout << "Press Ctrl+C to stop ..." << std::endl;

    hv::HttpServer server(&router);
    server.setPort(port);
    server.setThreadNum(4);
    server.run();
}
