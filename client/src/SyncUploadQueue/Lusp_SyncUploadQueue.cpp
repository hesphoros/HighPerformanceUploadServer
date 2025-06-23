// PIMPL方案：Lusp_SyncUploadQueuePrivate.h 只在 src/SyncUploadQueue/ 下 include，隐藏实现细节
#include "Lusp_SyncUploadQueue.h"
#include "ThreadSafeRowLockQueue/ThreadSafeRowLockQueue.hpp"
#include "Lusp_SyncUploadQueuePrivate.h"
#include "FileInfo/FileInfo.h"
#include "qglobal.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <vector>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

Lusp_SyncUploadQueue& Lusp_SyncUploadQueue::instance() {
    static Lusp_SyncUploadQueue instance;
    return instance;
}

Lusp_SyncUploadQueue::Lusp_SyncUploadQueue() : d(std::make_unique<Lusp_SyncUploadQueuePrivate>()) {
    std::cout << "Lusp_SyncUploadQueue: Global instance created" << std::endl;
}
Lusp_SyncUploadQueue::~Lusp_SyncUploadQueue() {
    std::cout << "Lusp_SyncUploadQueue: Global instance destroyed" << std::endl;
}
void Lusp_SyncUploadQueue::push(const std::u16string& filePath) {
    d->pushFile(filePath);
}
void Lusp_SyncUploadQueue::push(const std::vector<std::u16string>& filePaths) {
    d->pushFiles(filePaths);
}
void Lusp_SyncUploadQueue::push(const Lusp_SyncUploadFileInfo& fileInfo) {
    // 不能直接传结构体，需用路径构造handler
    Lusp_SyncUploadFileInfoHandler handler(fileInfo.sFileFullNameValue);
    d->pushFileInfo(handler);
}
void Lusp_SyncUploadQueue::setProgressCallback(ProgressCallback callback) {
    d->progressCallback = std::move(callback);
}
void Lusp_SyncUploadQueue::setCompletedCallback(CompletedCallback callback) {
    d->completedCallback = std::move(callback);
}
void Lusp_SyncUploadQueue::setAutoStart(bool autoStart) {
    d->m_autoStart = autoStart;
}
size_t Lusp_SyncUploadQueue::pendingCount() const {
    return d->uploadQueue.size();
}
bool Lusp_SyncUploadQueue::isActive() const {
    return d->m_isRunning.load();
}
bool Lusp_SyncUploadQueue::empty() const {
    return d->uploadQueue.empty();
}
