#pragma once
#include "FileInfo/FileInfo.h"
#include <QString>

/**
 * @brief Qt适配器：将Lusp_SyncUploadFileInfoHandler接口转换为Qt友好接口
 */
class FileInfoQtAdapter {
public:
    // 支持直接用Handler或文件路径构造
    FileInfoQtAdapter(const Lusp_SyncUploadFileInfoHandler& handler)
        : m_handler(handler) {}
    explicit FileInfoQtAdapter(const QString& filePath)
        : m_handler(filePath.toStdString()) {}
    explicit FileInfoQtAdapter(const Lusp_SyncUploadFileInfo& info)
        : m_handler(info) {}

    // Qt友好getter
    QString getId() const { return QString::fromStdString(m_handler.getId()); }
    QString getFilePath() const { return QString::fromStdString(m_handler.getFilePath()); }
    QString getFileName() const { return QString::fromStdString(m_handler.getFileName()); }
    qint64 getFileSize() const { return static_cast<qint64>(m_handler.getFileSize()); }
    QString getMd5Hash() const { return QString::fromStdString(m_handler.getMd5Hash()); }
    QString getClientDevice() const { return QString::fromStdString(m_handler.getClientDevice()); }
    uint64_t getUploadTimeStamp() const { return m_handler.getUploadTimeStamp(); }
    QString getDescription() const { return QString::fromStdString(m_handler.getDescription()); }
    QString getStatusText() const { return QString::fromStdString(m_handler.getStatusText()); }
    QString getFileTypeText() const { return QString::fromStdString(m_handler.getFileTypeText()); }
    QString getFileExistPolicyText() const { return QString::fromStdString(m_handler.getFileExistPolicyText()); }
    int getProgressPercentage() const { return m_handler.getProgressPercentage(); }
    QString getFormatUploadTimestamp() const { return QString::fromStdString(m_handler.getFormatUploadTimestamp()); }

    // Qt友好setter
    void setId(const QString& id) { m_handler.setId(id.toStdString()); }
    void setFilePath(const QString& path) { m_handler.setFileInfoPath(path.toStdString()); }
    void setFileName(const QString& name) { m_handler.setFileName(name.toStdString()); }
    void setMd5Hash(const QString& md5) { m_handler.setMd5Hash(md5.toStdString()); }
    void setClientDevice(const QString& device) { m_handler.setClientDevice(device.toStdString()); }
    void setDescription(const QString& desc) { m_handler.setDescription(desc.toStdString()); }
    void setRecordTime(const QString& recordTime) { m_handler.setRecordTime(recordTime.toStdString()); }

    // 获取原始Handler和结构体
    const Lusp_SyncUploadFileInfoHandler& getHandler() const { return m_handler; }
    Lusp_SyncUploadFileInfoHandler& getHandler() { return m_handler; }
    Lusp_SyncUploadFileInfo getFileInfoStruct() const { return m_handler.getFileInfoStruct(); }

private:
    Lusp_SyncUploadFileInfoHandler m_handler;
};
