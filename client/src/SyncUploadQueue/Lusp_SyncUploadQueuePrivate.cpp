#include "Lusp_SyncUploadQueuePrivate.h"
#include <filesystem>
#include "FileInfo/FileInfo.h"
#include "log_headers.h"
#include "log/UniConv.h"

Lusp_SyncUploadQueuePrivate::Lusp_SyncUploadQueuePrivate()
    : m_autoStart(true), m_isRunning(false), m_shouldStop(false) {}

Lusp_SyncUploadQueuePrivate::~Lusp_SyncUploadQueuePrivate() {
    cleanup();
}

void Lusp_SyncUploadQueuePrivate::cleanup() {
    m_shouldStop = true;
    m_isRunning = false;
}

void Lusp_SyncUploadQueuePrivate::pushFile(const std::u16string& filePath) {
    Lusp_SyncUploadFileInfoHandler handler(filePath);
    pushFileInfo(handler);
}

void Lusp_SyncUploadQueuePrivate::pushFiles(const std::vector<std::u16string>& filePaths) {
    for (const auto& path : filePaths) {
        pushFile(path);
    }
}

void Lusp_SyncUploadQueuePrivate::pushFileInfo(const Lusp_SyncUploadFileInfoHandler& handler) {
    // 入队前补全上传时间戳
    const_cast<Lusp_SyncUploadFileInfoHandler&>(handler).setCurrentTimestampMs();
    const auto& fileInfo = handler.getFileInfo();
    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO_U16,
        u"Dequeue: " +  fileInfo.sFileFullNameValue + u" (Type: " + handler.getFileTypeTextU16() + u")"
    );
    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO,
        "File Size: " + std::to_string(fileInfo.sSyncFileSizeValue) + " bytes"
    );
    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO,
        "File MD5: " + fileInfo.sFileMd5ValueInfo
    );
    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO,
        "File Record Time: " + fileInfo.sFileRecordTimeValue
    );
    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO,
        "File Upload Timestamp: " + std::to_string(fileInfo.uUploadTimeStamp)
    );
    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO,
        "File Exist Policy: " + handler.getFileExistPolicyText()
    );
    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO,
        "File Status: " + handler.getStatusText()
    );
    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO,
        "File Description: " +  LUSP_UNICONV->ToLocaleFromUtf16LE (fileInfo.sDescriptionInfo )
    );
    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO,
        "File Enqueue Time: " + handler.getFormatUploadTimestamp() // 使用当前时间作为入队时间
    );
    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO,
        "File Client Device: " + LUSP_UNICONV->ToLocaleFromUtf16LE(fileInfo.sLanClientDevice)
    );
    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO,
        "File Auth Token: " + fileInfo.sAuthTokenValues
    );

    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO,
        "File Only Name: " +  LUSP_UNICONV->ToLocaleFromUtf16LE (fileInfo.sOnlyFileNameValue  )// 使用文件名作为唯一标识
    );

    g_LogSyncUploadQueueInfo.WriteLogContent(
        LOG_INFO,
        "M-ID " + handler.getId()
    );

    uploadQueue.push(fileInfo);
    if (completedCallbackU16) {
        completedCallbackU16(fileInfo.sFileFullNameValue, true, u"文件已入队");
    }
    if (progressCallbackU16) {
        progressCallbackU16(fileInfo.sFileFullNameValue, 0, u"等待上传");
    }
}
