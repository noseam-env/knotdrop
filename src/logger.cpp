/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "logger.h"

#if defined(ANDROID)
#include <android/log.h>
#define LOG_TAG "JNI"
#else
#include <iostream>
#endif

bool Logger::debug = false;

void Logger::set_debug(bool enabled) {
    debug = enabled;
}

void Logger::log(Logger::LogLevel level, const std::string &message) {
    if (level == LEVEL_DEBUG) {
        if (!debug) return;
        level = LEVEL_INFO;
    }
#if defined(ANDROID)
    __android_log_print(level == LEVEL_ERROR ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, LOG_TAG, "%s", message.c_str());
#else
    if (level == LEVEL_INFO) {
        std::cout << message << std::endl;
    } else if (level == LEVEL_ERROR) {
        std::cerr << message << std::endl;
    }
#endif
}

