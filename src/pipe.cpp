/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#include "pipe.hpp"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <corecrt_io.h>
#include <fcntl.h>

#define read _read
#define write _write
#define close _close
#else
#include <unistd.h>
#endif

#ifndef _SSIZE_T_DEFINED
#  if defined(__POCC__) || defined(__MINGW32__)
#  elif defined(_WIN64)
#    define _SSIZE_T_DEFINED
#    define ssize_t __int64
#  else
#    define _SSIZE_T_DEFINED
#    define ssize_t int
#  endif
#endif


Pipe::Pipe() {
    CreatePipe();
}

Pipe::~Pipe() {
    Close();
}

std::string Pipe::ReadStr() {
    std::string data;
    char buffer[256];
    ssize_t bytesRead;

    while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        data += std::string(buffer, bytesRead);
    }

    return data;
}

int Pipe::GetReadFD() {
    return pipefd[0];
}

bool Pipe::Write(const std::string& data) {
    return Pipe::Write(data.c_str(), data.size());
}

bool Pipe::Write(const char *data, size_t size) {
    std::cout << "fh: " << std::to_string(pipefd[1]) << std::endl;
    ssize_t bytesWritten = write(pipefd[1], data, static_cast<unsigned int>(size));
    return bytesWritten > 0;
}

void Pipe::Close() {
    close(pipefd[0]);
    close(pipefd[1]);
}

void Pipe::CreatePipe() {
#ifdef _WIN32
    /*if (_pipe(pipefd, 8192, _O_BINARY) == -1) {
        std::cout << "Не удалось создать пайп." << std::endl;
    }*/

    HANDLE hReadPipe, hWritePipe;
    if (::CreatePipe(&hReadPipe, &hWritePipe, nullptr, 0)) {
        pipefd[0] = _open_osfhandle(reinterpret_cast<intptr_t>(hReadPipe), 0);
        pipefd[1] = _open_osfhandle(reinterpret_cast<intptr_t>(hWritePipe), 1);
    }
#else
        if (pipe(pipefd) == 0) {
            // Успешно создан пайп
        }
#endif
}

