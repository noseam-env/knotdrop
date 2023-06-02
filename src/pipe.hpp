/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#ifndef LIBFLOWDROP_PIPE_HPP
#define LIBFLOWDROP_PIPE_HPP


#include <iostream>
#include <sstream>

class Pipe {
public:
    Pipe();
    ~Pipe();

    std::string ReadStr();
    int GetReadFD();
    bool Write(const std::string& data);
    bool Write(const char *data, size_t size);
    void Close();

private:
    int pipefd[2] = { -1, -1 };

    void CreatePipe();
};


#endif //LIBFLOWDROP_PIPE_HPP
