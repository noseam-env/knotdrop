/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */
#pragma once

#include "string"

class Logger {
public:
    enum LogLevel {
        LEVEL_INFO,
        LEVEL_DEBUG,
        LEVEL_ERROR
    };

    static void set_debug(bool enabled);

    static void log(LogLevel, const std::string&);

private:
    static bool debug;
};

