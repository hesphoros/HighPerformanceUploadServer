#ifndef FILEINFO_QT_ADAPTER_H
#define FILEINFO_QT_ADAPTER_H

#include "FileInfo.h"
#include <QString>

/**
 * @brief Qt适配器：将Lusp_SyncUploadFileInfoHandler接口转换为Qt友好接口
 */
class FileInfoQtAdapter {
public:
    // 只允许用文件路径或Handler构造，不允许结构体构造
    FileInfoQtAdapter(const Lusp_SyncUploadFileInfoHandler& handler)
        : m_handler(handler) {
    }
    explicit FileInfoQtAdapter(const QString& filePath)
        : m_handler(filePath.toStdU16String()) {
    }
    // explicit FileInfoQtAdapter(const Lusp_SyncUploadFileInfo& info) = delete;

    // Qt友好getter
    QString getId() const { return QString::fromStdString(m_handler.getId()); }
    QString getFilePath() const { return QString::fromStdString(m_handler.getFilePath()); }
    // 使用 UTF-16 直接构造 QString，避免编码转换问题
    QString getFileName() const {
        const std::u16string& u16name = m_handler.getFileNameU16();
        return QString::fromUtf16(u16name.data(), static_cast<int>(u16name.size()));
    }
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

    // 不再暴露setter接口
    // 获取原始Handler和结构体
    const Lusp_SyncUploadFileInfoHandler& getHandler() const { return m_handler; }
    Lusp_SyncUploadFileInfoHandler& getHandler() { return m_handler; }
    Lusp_SyncUploadFileInfo getFileInfoStruct() const { return m_handler.getFileInfoStruct(); }

private:
    Lusp_SyncUploadFileInfoHandler m_handler;
};


#endif // FILEINFO_QT_ADAPTER_H