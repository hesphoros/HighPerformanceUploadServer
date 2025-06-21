#pragma once

#include <QString>
#include <QDateTime>
#include <QFileInfo>
#include "FileInfo/FileInfo.h"

/**
 * @brief Qt兼容的FileInfo包装类
 * 基于Lusp_SyncUploadFileInfo结构体，提供Qt风格的接口
 */
class FileInfo {
public:
    // 构造函数
    FileInfo();
    explicit FileInfo(const QString& filePath);
    FileInfo(const Lusp_SyncUploadFileInfo& luspFileInfo);
    
    // 析构函数
    ~FileInfo() = default;

    // 基本属性访问器
    QString getId() const { return m_id; }
    QString getFilePath() const { return QString::fromStdString(m_luspFileInfo.sFileFullNameValue); }
    QString getFileName() const { return QString::fromStdString(m_luspFileInfo.sOnlyFileNameValue); }
    qint64 getFileSize() const { return static_cast<qint64>(m_luspFileInfo.sSyncFileSizeValue); }
    QString getMd5Hash() const { return QString::fromStdString(m_luspFileInfo.sFileMd5ValueInfo); }
    QString getClientDevice() const { return QString::fromStdString(m_luspFileInfo.sLanClientDevice); }
    uint64_t getUploadTimeStamp() const { return m_luspFileInfo.uUploadTimeStamp; }
    QString getDescription() const { return QString::fromStdString(m_luspFileInfo.sDescriptionInfo); }
    
    // 设置器
    void setId(const QString& id) { m_id = id; }
    void setFilePath(const QString& path);
    void setFileName(const QString& name) { m_luspFileInfo.sOnlyFileNameValue = name.toStdString(); }
    void setFileSize(qint64 size) { m_luspFileInfo.sSyncFileSizeValue = static_cast<size_t>(size); }
    void setMd5Hash(const QString& md5) { m_luspFileInfo.sFileMd5ValueInfo = md5.toStdString(); }
    void setClientDevice(const QString& device) { m_luspFileInfo.sLanClientDevice = device.toStdString(); }
    void setUploadTimeStamp(uint64_t timestamp) { m_luspFileInfo.uUploadTimeStamp = timestamp; }
    void setDescription(const QString& desc) { m_luspFileInfo.sDescriptionInfo = desc.toStdString(); }
    
    // 状态相关
    Lusp_UploadStatusInf getUploadStatus() const { return m_luspFileInfo.eUploadStatusInf; }
    void setUploadStatus(Lusp_UploadStatusInf status) { m_luspFileInfo.eUploadStatusInf = status; }
    QString getStatusText() const;
    
    // 文件类型相关
    Lusp_UploadFileTyped getFileType() const { return m_luspFileInfo.eUploadFileTyped; }
    void setFileType(Lusp_UploadFileTyped type) { m_luspFileInfo.eUploadFileTyped = type; }
    QString getFileTypeText() const;
    
    // 文件存在策略
    Lusp_FileExistPolicy getFileExistPolicy() const { return m_luspFileInfo.eFileExistPolicy; }
    void setFileExistPolicy(Lusp_FileExistPolicy policy) { m_luspFileInfo.eFileExistPolicy = policy; }
    QString getFileExistPolicyText() const;
    
    // 上传进度相关
    qint64 getUploadedBytes() const { return m_uploadedBytes; }
    void setUploadedBytes(qint64 bytes) { m_uploadedBytes = bytes; }
    int getProgressPercentage() const;
    
    // 时间相关
    QDateTime getAddTime() const { return m_addTime; }
    void setAddTime(const QDateTime& time) { m_addTime = time; }
    QString getRecordTime() const { return QString::fromStdString(m_luspFileInfo.sFileRecordTimeValue); }
    void setRecordTime(const QString& time) { m_luspFileInfo.sFileRecordTimeValue = time.toStdString(); }
    
    // 工具方法
    void updateFromFileSystem();
    void calculateMd5();
    Lusp_UploadFileTyped detectFileType(const QString& filePath);
    
    // 获取原始结构体
    const Lusp_SyncUploadFileInfo& getLuspFileInfo() const { return m_luspFileInfo; }
    Lusp_SyncUploadFileInfo& getLuspFileInfo() { return m_luspFileInfo; }

private:
    QString m_id;                           // Qt UI用的唯一标识
    Lusp_SyncUploadFileInfo m_luspFileInfo; // 核心数据结构
    qint64 m_uploadedBytes;                 // 已上传字节数(UI临时状态)
    QDateTime m_addTime;                    // 添加到队列的时间
    
    void initializeDefaults();
    QString getCurrentTimeString() const;
    QString getComputerName() const;
};
