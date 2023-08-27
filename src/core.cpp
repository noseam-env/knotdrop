/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "flowdrop/flowdrop.hpp"
#include "core.h"
#include <utility>
#include <sstream>
#include <iostream>
#include "os/file_info.h"
#include "logger.h"
#include "fstream"

#if defined(__clang__)
#include "sys/stat.h"
#endif

void getJsonOptionalString(const json &j, const std::string &key, std::optional<std::string> &strOpt) {
    if (j.count(key) != 0) {
        std::string strValue;
        j.at(key).get_to(strValue);
        if (!strValue.empty()) {
            strOpt = std::move(strValue);
        }
    }
}

void setJsonOptionalString(json &j, const std::string &key, const std::optional<std::string> &strOpt) {
    if (strOpt.has_value()) {
        j[key] = *strOpt;
    }
}

namespace flowdrop {

    void setDebug(bool enabled) {
        Logger::set_debug(enabled);
    }

    std::string generate_md5_id() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        std::ostringstream os;
        os << ts;
        std::string ts_str = os.str();

        std::string hash;
        {
            std::array<uint8_t, 16> md5_hash{};
            std::memset(md5_hash.data(), 0, md5_hash.size());

            std::hash<std::string> hash_fn;
            std::size_t hash_value = hash_fn(ts_str);
            std::memcpy(md5_hash.data(), &hash_value, sizeof(hash_value));

            hash.resize(md5_hash.size() * 2);
            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            for (auto b: md5_hash) {
                ss << std::setw(2) << static_cast<int>(b);
            }
            hash = ss.str();
        }

        return hash.substr(0, 12);
    }

    void to_json(json &j, const DeviceInfo &d) {
        j["id"] = d.id;
        setJsonOptionalString(j, "name", d.name);
        setJsonOptionalString(j, "model", d.model);
        setJsonOptionalString(j, "platform", d.platform);
        setJsonOptionalString(j, "system_version", d.system_version);
    }

    void from_json(const json &j, DeviceInfo &d) {
        j.at("id").get_to(d.id);
        getJsonOptionalString(j, "name", d.name);
        getJsonOptionalString(j, "model", d.model);
        getJsonOptionalString(j, "platform", d.platform);
        getJsonOptionalString(j, "system_version", d.system_version);
    }

    void to_json(json &j, const FileInfo &d) {
        j["name"] = d.name;
        j["size"] = d.size;
    }

    void from_json(const json &j, FileInfo &d) {
        j.at("name").get_to(d.name);
        j.at("size").get_to(d.size);
    }

    void to_json(json &j, const SendAsk &d) {
        j["sender"] = d.sender;
        j["files"] = d.files;
    }

    void from_json(const json &j, SendAsk &d) {
        j.at("sender").get_to(d.sender);
        j.at("files").get_to(d.files);
    }

    class NativeFile::Impl {
    public:
        Impl(std::filesystem::path filePath, std::string relativePath) :
                _filePath(std::move(filePath)),
                _relativePath(std::move(relativePath)),
                _fileStream(_filePath, std::ios::binary | std::ios::in) {
            if (!_fileStream) {
                throw std::runtime_error("Error opening file: " + _filePath.string());
            }
            int result = knotdrop_util_fileinfo(_filePath.string().c_str(), &_createdTime, &_modifiedTime);
            if (result == 1) {
                Logger::log(Logger::LEVEL_ERROR, "knotdrop_util_fileinfo: error opening file");
            } else if (result == 2) {
                Logger::log(Logger::LEVEL_ERROR, "knotdrop_util_fileinfo: error getting file time");
            }
            if (_createdTime == 0) {
                _createdTime = std::time(nullptr);
            }
            if (_modifiedTime == 0) {
                _modifiedTime = _createdTime;
            }
        }
        ~Impl() = default;

        [[nodiscard]] std::string getRelativePath() const {
            return _relativePath;
        }

        [[nodiscard]] std::uint64_t getSize() const {
            return std::filesystem::file_size(_filePath);
        }

        [[nodiscard]] std::uint64_t getCreatedTime() const {
            return _createdTime;
        }

        [[nodiscard]] std::uint64_t getModifiedTime() const {
            return _modifiedTime;
        }

        [[nodiscard]] std::filesystem::file_status getStatus() const {
            return std::filesystem::status(_filePath);
        }

        void seek(std::uint64_t pos) {
            _fileStream.seekg(static_cast<std::ifstream::pos_type>(pos));
        }

        std::uint64_t read(char *buffer, std::uint64_t count) {
            _fileStream.read(buffer, static_cast<std::streamsize>(count));
            return static_cast<std::uint64_t>(_fileStream.gcount());
        }

    private:
        std::filesystem::path _filePath;
        std::string _relativePath;
        std::uint64_t _createdTime;
        std::uint64_t _modifiedTime;
        std::ifstream _fileStream;
    };

    NativeFile::NativeFile(const std::filesystem::path& filePath, std::string relativePath) : pImpl(new Impl(filePath, std::move(relativePath))) {}
    NativeFile::~NativeFile() = default;
    std::string NativeFile::getRelativePath() const {
        return pImpl->getRelativePath();
    }
    std::uint64_t NativeFile::getSize() const {
        return pImpl->getSize();
    }
    std::uint64_t NativeFile::getCreatedTime() const {
        return pImpl->getCreatedTime();
    }
    std::uint64_t NativeFile::getModifiedTime() const {
        return pImpl->getModifiedTime();
    }
    std::filesystem::perms NativeFile::getPermissions() const {
        return pImpl->getStatus().permissions();
    }
    void NativeFile::seek(std::uint64_t pos) {
        return pImpl->seek(pos);
    }
    std::uint64_t NativeFile::read(char* buffer, std::uint64_t count) {
        return pImpl->read(buffer, count);
    }

}
