/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */
#pragma once

#include "flowdrop/flowdrop.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace flowdrop {
    void to_json(json &j, const DeviceInfo &d);
    void from_json(const json &j, DeviceInfo &d);

    void to_json(json &j, const FileInfo &d);
    void from_json(const json &j, FileInfo &d);

    void to_json(json &j, const SendAsk &d);
    void from_json(const json &j, SendAsk &d);
}
