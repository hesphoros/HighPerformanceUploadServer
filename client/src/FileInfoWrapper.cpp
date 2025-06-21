#include "FileInfoWrapper.h"
#include <QCryptographicHash>
#include <QUuid>
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <windows.h>

FileInfo::FileInfo() 
    : m_uploadedBytes(0)
    , m_addTime(QDateTime::currentDateTime()) {
    initializeDefaults();
}

FileInfo::FileInfo(const QString& filePath) 
    : m_uploadedBytes(0)
    , m_addTime(QDateTime::currentDateTime()) {
    initializeDefaults();
    setFilePath(filePath);
    updateFromFileSystem();
}

FileInfo::FileInfo(const Lusp_SyncUploadFileInfo& luspFileInfo)
    : m_luspFileInfo(luspFileInfo)
    , m_uploadedBytes(0)
    , m_addTime(QDateTime::currentDateTime()) {
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void FileInfo::initializeDefaults() {
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_luspFileInfo.eUploadFileTyped = Lusp_UploadFileTyped::LUSP_UPLOADTYPE_UNDEFINED;
    m_luspFileInfo.sLanClientDevice = getComputerName().toStdString();
    m_luspFileInfo.sSyncFileSizeValue = 0;
    m_luspFileInfo.eFileExistPolicy = Lusp_FileExistPolicy::LUSP_FILE_EXIST_POLICY_OVERWRITE;
    m_luspFileInfo.uUploadTimeStamp = QDateTime::currentSecsSinceEpoch();
    m_luspFileInfo.eUploadStatusInf = Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_PENDING;
    m_luspFileInfo.sFileRecordTimeValue = getCurrentTimeString().toStdString();
}

void FileInfo::setFilePath(const QString& path) {
    m_luspFileInfo.sFileFullNameValue = QDir::toNativeSeparators(path).toStdString();
    QFileInfo fileInfo(path);
    m_luspFileInfo.sOnlyFileNameValue = fileInfo.fileName().toStdString();
    m_luspFileInfo.eUploadFileTyped = detectFileType(path);
}

void FileInfo::updateFromFileSystem() {
    QString filePath = getFilePath();
    if (filePath.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qWarning() << "文件不存在:" << filePath;
        return;
    }

    // 更新文件大小
    setFileSize(fileInfo.size());
    
    // 更新文件名（可能路径中的文件名与实际文件名不同）
    setFileName(fileInfo.fileName());
    
    // 更新记录时间
    setRecordTime(fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
    
    // 如果MD5为空，计算MD5
    if (getMd5Hash().isEmpty()) {
        calculateMd5();
    }
}

void FileInfo::calculateMd5() {
    QString filePath = getFilePath();
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开文件计算MD5:" << filePath;
        return;
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    while (!file.atEnd()) {
        QByteArray data = file.read(8192); // 8KB块读取
        hash.addData(data);
    }
    
    QString md5 = hash.result().toHex();
    setMd5Hash(md5);
    qDebug() << "文件MD5计算完成:" << getFileName() << "MD5:" << md5;
}

Lusp_UploadFileTyped FileInfo::detectFileType(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    // 文档类型
    if (suffix == "txt" || suffix == "doc" || suffix == "docx" || 
        suffix == "pdf" || suffix == "rtf" || suffix == "odt") {
        return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_DOCUMENT;
    }
    
    // 图片类型
    if (suffix == "jpg" || suffix == "jpeg" || suffix == "png" || 
        suffix == "gif" || suffix == "bmp" || suffix == "svg" || 
        suffix == "tiff" || suffix == "webp") {
        return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_IMAGE;
    }
    
    // 视频类型
    if (suffix == "mp4" || suffix == "avi" || suffix == "mkv" || 
        suffix == "mov" || suffix == "wmv" || suffix == "flv" || 
        suffix == "webm" || suffix == "m4v") {
        return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_VIDEO;
    }
    
    // 音频类型
    if (suffix == "mp3" || suffix == "wav" || suffix == "flac" || 
        suffix == "aac" || suffix == "ogg" || suffix == "wma" || 
        suffix == "m4a") {
        return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_AUDIO;
    }
    
    // 压缩包类型
    if (suffix == "zip" || suffix == "rar" || suffix == "7z" || 
        suffix == "tar" || suffix == "gz" || suffix == "bz2" || 
        suffix == "xz") {
        return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_ARCHIVE;
    }
    
    // 代码文件类型
    if (suffix == "cpp" || suffix == "h" || suffix == "c" || 
        suffix == "hpp" || suffix == "js" || suffix == "py" || 
        suffix == "java" || suffix == "cs" || suffix == "php" || 
        suffix == "html" || suffix == "css" || suffix == "xml" || 
        suffix == "json") {
        return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_CODE;
    }
    
    return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_UNDEFINED;
}

QString FileInfo::getStatusText() const {
    switch (m_luspFileInfo.eUploadStatusInf) {
    case Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_COMPLETED:
        return "上传完成";
    case Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_PENDING:
        return "等待上传";
    case Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_UPLOADING:
        return "正在上传";
    case Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_REJECTED:
        return "上传被拒绝";
    case Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_FAILED:
        return "上传失败";
    case Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_UNDEFINED:
    default:
        return "未知状态";
    }
}

QString FileInfo::getFileTypeText() const {
    switch (m_luspFileInfo.eUploadFileTyped) {
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_DOCUMENT:
        return "文档";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_IMAGE:
        return "图片";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_VIDEO:
        return "视频";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_AUDIO:
        return "音频";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_ARCHIVE:
        return "压缩包";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_CODE:
        return "代码";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_UNDEFINED:
    default:
        return "未知类型";
    }
}

QString FileInfo::getFileExistPolicyText() const {
    switch (m_luspFileInfo.eFileExistPolicy) {
    case Lusp_FileExistPolicy::LUSP_FILE_EXIST_POLICY_OVERWRITE:
        return "覆盖";
    case Lusp_FileExistPolicy::LUSP_FILE_EXIST_POLICY_SKIP:
        return "跳过";
    case Lusp_FileExistPolicy::LUSP_FILE_EXIST_POLICY_RENAME:
        return "重命名";
    case Lusp_FileExistPolicy::LUSP_FILE_EXIST_POLICY_UNDEFINED:
    default:
        return "未定义";
    }
}

int FileInfo::getProgressPercentage() const {
    if (m_luspFileInfo.sSyncFileSizeValue == 0) {
        return 0;
    }
    return static_cast<int>((m_uploadedBytes * 100) / static_cast<qint64>(m_luspFileInfo.sSyncFileSizeValue));
}

QString FileInfo::getCurrentTimeString() const {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
}

QString FileInfo::getComputerName() const {
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    
    if (GetComputerNameA(computerName, &size)) {
        return QString::fromLocal8Bit(computerName);
    } else {
        qWarning() << "无法获取计算机名称，错误码:" << GetLastError();
        return "Unknown";
    }
}
