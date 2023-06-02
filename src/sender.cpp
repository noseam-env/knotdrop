/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#include "../include/flowdrop.hpp"
#include "hv/axios.h"
#include "static_libs.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <sys/stat.h>
#include <future>
#include "specification.hpp"
#include "curl/curl.h"


bool ask(const std::string &baseUrl, const std::vector<std::string> &files, int timeout) {
    std::vector<flowdrop::FileInfo> fileInfoVec(files.size());
    for (size_t i = 0; i < files.size(); ++i) {
        std::ifstream file(files[i], std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cout << "Failed to open file: " << files[i] << std::endl;
            return false;
        }
        std::streampos fileSize = file.tellg();
        fileInfoVec[i].name = files[i];
        fileInfoVec[i].size = static_cast<int64_t>(fileSize);
        file.close();
    }

    flowdrop::SendAsk askIn;
    askIn.sender = flowdrop::thisDeviceInfo;
    askIn.files = fileInfoVec;

    std::string jsonData = json(askIn).dump();

    Request req(new HttpRequest);
    req->method = HTTP_POST;
    req->url = baseUrl + flowdrop_endpoint_ask;
    req->timeout = timeout;
    req->body = jsonData;
    req->headers["Content-Type"] = std::to_string(APPLICATION_JSON);
    auto resp = requests::request(req);
    if (resp == nullptr) {
        //printf("request failed!\n");
        return false;
    }

    json responseJson = json::parse(resp->body);

    return responseJson["accepted"].get<bool>();
}

long sizeFILE(FILE *file) {
    long fileSize = 0;
    if (file != nullptr) {
        long currentPosition = std::ftell(file);
        std::fseek(file, 0, SEEK_END);
        fileSize = std::ftell(file);
        std::fseek(file, currentPosition, SEEK_SET);
    }
    return fileSize;
}

void packFile(archive *a, const std::string &filePath) {
    std::filesystem::path path(filePath);

    FILE* file = fopen(filePath.c_str(), "rb");
    if (!file) {
        std::cerr << "Failed to open file: " << strerror(errno) << std::endl;
        return;
    }

    struct archive_entry *entry = archive_entry_new();

    archive_entry_set_pathname(entry, path.filename().string().c_str());
    archive_entry_set_size(entry, sizeFILE(file));
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);

    char buff[8192];

    size_t len = fread(buff, 1, sizeof(buff), file);
    while (len > 0) {
        archive_write_data(a, buff, len);
        len = fread(buff, 1, sizeof(buff), file);
    }
    fclose(file);

    archive_entry_free(entry);
}

void createTar(const std::vector<std::string> &files, FILE *outputFile) {
    struct archive *a = archive_write_new();
    //archive_write_set_format_ustar(a);
    archive_write_set_format_pax_restricted(a);

    if (archive_write_open_FILE(a, outputFile) != ARCHIVE_OK) {
        std::cerr << archive_error_string(a) << std::endl;
    }

    for (const auto &file: files) {
        packFile(a, file);
        //std::cout << file << std::endl;
    }

    archive_write_close(a);
    archive_write_free(a);
}

HV_INLINE Response uploadLargeFile(const char *url, FILE *fp, requests::upload_progress_cb progress_cb = NULL,
                                   http_method method = HTTP_POST, const http_headers &headers = DefaultHeaders) {
    // open file
    HFile file;
    file.fp = fp;

    hv::HttpClient cli;
    Request req(new HttpRequest);
    req->method = method;
    req->url = url;
    req->timeout = 3600; // 1h
    if (&headers != &DefaultHeaders) {
        req->headers = headers;
    }

    // connect
    req->ParseUrl();
    //std::cout << "host: " << req->host << std::endl;
    //std::cout << "port: " << std::to_string(req->port) << std::endl;
    //std::cout << "url: " << url << std::endl;
    int connfd = cli.connect(req->host.c_str(), req->port, req->IsHttps(), req->connect_timeout);
    if (connfd < 0) {
        std::cout << "135" << std::endl;
        return NULL;
    }

    // send header
    size_t total_bytes = sizeFILE(file.fp);
    req->SetHeader("Content-Length", hv::to_string(total_bytes));
    req->SetHeader("Transfer-Encoding", "chunked");
    req->SetHeader("Content-Type", "application/x-tar");
    int ret = cli.sendHeader(req.get());
    if (ret != 0) {
        std::cout << "146" << std::endl;
        return NULL;
    }

    // send file
    size_t sended_bytes = 0;
    char filebuf[40960]; // 40K
    int filebuflen = sizeof(filebuf);
    int nread, nsend;
    while (sended_bytes < total_bytes) {
        nread = file.read(filebuf, filebuflen);
        if (nread <= 0) {
            std::cout << "158" << std::endl;
            return NULL;
        }
        nsend = cli.sendData(filebuf, nread);
        if (nsend != nread) {
            std::cout << "163" << std::endl;
            return NULL;
        }
        sended_bytes += nsend;
        if (progress_cb) {
            progress_cb(sended_bytes, total_bytes);
        }
    }

    // recv response
    Response resp(new HttpResponse);
    ret = cli.recvResponse(resp.get());
    if (ret != 0) {
        std::cout << "176" << std::endl; // HERE
        return NULL;
    }
    return resp;
}

size_t ignoreDataCallback(char* /*buffer*/, size_t size, size_t nmemb, void* /*userdata*/) {
    return size * nmemb;
}

void sendFiles(const std::string &baseUrl, const std::vector<std::string> &files) {
    FILE *tmpFile = std::tmpfile();

    createTar(files, tmpFile);

    std::fseek(tmpFile, 0, SEEK_SET);

    /*auto resp = uploadLargeFile((baseUrl + flowdrop_endpoint_send).c_str(), tmpFile);
    if (resp == nullptr) {
        printf("Request failed!\n");
    }*/

    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, (baseUrl + flowdrop_endpoint_send).c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_READDATA, tmpFile);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, sizeFILE(tmpFile));

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ignoreDataCallback);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "Send file error: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
    }

    fclose(tmpFile);
}

bool askAndSend(const flowdrop::Address &address, const std::vector<std::string> &files, int askTimeout) {
    std::string baseUrl = "http://" + address.host + ":" + std::to_string(address.port) + "/";

    std::cout << "Asking receiver to accept ..." << std::endl;

    if (!ask(baseUrl, files, askTimeout)) {
        std::cout << "Receiver declined" << std::endl;
        return false;
    }

    std::cout << "Receiver accepted" << std::endl;

    sendFiles(baseUrl, files);

    std::cout << "Done!" << std::endl;
    return true;
}

void flowdrop::send(const std::string &receiverId, const std::vector<std::string> &files, int resolveTimeout,
                    int askTimeout) {
    std::cout << "Resolving receiver ..." << std::endl;

    bool resolved = false;
    std::promise<Address> addressPromise;
    std::future<Address> addressFuture = addressPromise.get_future();

    std::thread resolveThread([&resolved, receiverId, &addressPromise](const std::vector<std::string> &files) {
        flowdrop::resolve(receiverId, [&resolved, &addressPromise](const Address &address) {
            resolved = true;
            addressPromise.set_value(address);
        });
    }, files);

    std::future_status status = addressFuture.wait_for(std::chrono::seconds(resolveTimeout));

    if (status == std::future_status::ready) {
        Address address = addressFuture.get();
        std::cout << "Resolved " << address.host << ":" << std::to_string(address.port) << std::endl;
        askAndSend(address, files, askTimeout);
    } else {
        std::cout << "Receiver not found" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    resolveThread.join();
}

