/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/FlowDrop/libflowdrop/blob/master/LEGAL
 */

#ifndef LIBFLOWDROP_FLOWDROP_H
#define LIBFLOWDROP_FLOWDROP_H

#ifdef __cplusplus
extern "C" {
#endif

#define FD_EXPORT __declspec(dllexport)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct DeviceInfo {
    char* id;
    char* uuid;
    char* name;
    char* model;
    char* platform;
    char* system_version;
};

FD_EXPORT char* generate_md5_id();

struct FileInfo {
    char* name;
    size_t size;
};

struct SendAsk {
    DeviceInfo sender;
    FileInfo* files;
    size_t file_count;
};

typedef struct {
    void (*onResolving)();
    void (*onReceiverNotFound)();
    void (*onResolved)();
    void (*onAskingReceiver)();
    void (*onReceiverDeclined)();
    void (*onReceiverAccepted)();
    void (*onSendingStart)();
    void (*onSendingTotalProgress)(size_t totalSize, size_t currentSize);
    void (*onSendingFileStart)(const FileInfo* fileInfo);
    void (*onSendingFileProgress)(const FileInfo* fileInfo, size_t currentSize);
    void (*onSendingFileEnd)(const FileInfo* fileInfo);
    void (*onSendingEnd)();
    void (*onReceiverStarted)(unsigned short port);
    void (*onSenderAsk)(const DeviceInfo* sender);
    void (*onReceivingStart)(const DeviceInfo* sender, size_t totalSize);
    void (*onReceivingTotalProgress)(const DeviceInfo* sender, size_t totalSize, size_t receivedSize);
    void (*onReceivingFileStart)(const DeviceInfo* sender, const FileInfo* fileInfo);
    void (*onReceivingFileProgress)(const DeviceInfo* sender, const FileInfo* fileInfo, size_t receivedSize);
    void (*onReceivingFileEnd)(const DeviceInfo* sender, const FileInfo* fileInfo);
    void (*onReceivingEnd)(const DeviceInfo* sender, size_t totalSize);
} IEventListener;

typedef void (*findCallback)(const DeviceInfo*);

FD_EXPORT void find(findCallback callback);

// SendRequest

struct SendRequest;

FD_EXPORT SendRequest* SendRequest_SendRequest();

DeviceInfo* SendRequest_getDeviceInfo(const SendRequest* request);
SendRequest* SendRequest_setDeviceInfo(SendRequest* request, const DeviceInfo* info);

char* SendRequest_getReceiverId(const SendRequest* request);
SendRequest* SendRequest_setReceiverId(SendRequest* request, const char* id);

char** SendRequest_getFiles(const SendRequest* request);
size_t SendRequest_getFileCount(const SendRequest* request);
SendRequest* SendRequest_setFiles(SendRequest* request, const char** files, size_t file_count);

unsigned long SendRequest_getResolveTimeout(const SendRequest* request);
SendRequest* SendRequest_setResolveTimeout(SendRequest* request, unsigned long timeout);

unsigned long SendRequest_getAskTimeout(const SendRequest* request);
SendRequest* SendRequest_setAskTimeout(SendRequest* request, unsigned long timeout);

IEventListener* SendRequest_getEventListener(const SendRequest* request);
SendRequest* SendRequest_setEventListener(SendRequest* request, IEventListener* listener);

FD_EXPORT bool SendRequest_execute(SendRequest* request);

// Receiver

typedef bool (*askCallback)(const SendAsk*);

struct Receiver;

FD_EXPORT Receiver* Receiver_Receiver(DeviceInfo* deviceInfo);

FD_EXPORT const DeviceInfo* Receiver_getDeviceInfo(const Receiver* receiver);

FD_EXPORT const char* Receiver_getDestDir(const Receiver* receiver);

FD_EXPORT void Receiver_setDestDir(Receiver* receiver, const char* destDir);

FD_EXPORT askCallback Receiver_getAskCallback(const Receiver* receiver);

FD_EXPORT void Receiver_setAskCallback(Receiver* receiver, askCallback callback);

FD_EXPORT IEventListener* Receiver_getEventListener(const Receiver* receiver);

FD_EXPORT void Receiver_setEventListener(Receiver* receiver, IEventListener* listener);

FD_EXPORT void Receiver_run(Receiver* receiver, bool wait);

FD_EXPORT void Receiver_stop(Receiver* receiver);

#ifdef __cplusplus
}
#endif

#endif //LIBFLOWDROP_FLOWDROP_H
