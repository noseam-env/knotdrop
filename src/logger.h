//
// Created by nelon on 7/29/2023.
//
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

