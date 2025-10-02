#ifndef LUSP_SYNC_UPLOAD_FILE_INFO_H
#define LUSP_SYNC_UPLOAD_FILE_INFO_H

#include <QString>
#include <QDateTime>
#include <QFileInfo>
#include "hash-library/md5.h"
#include <windows.h>
#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <rpc.h>
#include <string>
#include <filesystem>

#include "log_headers.h"
#include "utils/EnumConvert.hpp"

/**
 * @brief 文件上传状态
 * @enum Lusp_UploadStatusInf
 */
enum class Lusp_UploadStatusInf {
    LUSP_UPLOAD_STATUS_IDENTIFIERS_COMPLETED   = 0,  /*!< 成功 */
    LUSP_UPLOAD_STATUS_IDENTIFIERS_PENDING     = 1,  /*!< 等待上传 */
    LUSP_UPLOAD_STATUS_IDENTIFIERS_UPLOADING   = 2,  /*!< 上传部分 */
    LUSP_UPLOAD_STATUS_IDENTIFIERS_REJECTED    = 3,  /*!< 拒绝上传 */
    LUSP_UPLOAD_STATUS_IDENTIFIERS_FAILED      = 4,  /*!< 失败 */
    LUSP_UPLOAD_STATUS_IDENTIFIERS_UNDEFINED   = 5   /*!< 未定义 */
};

/**
 * @brief 上传文件类型
 * @struct Lusp_UploadFileTyped
 */
enum class Lusp_UploadFileTyped {
    LUSP_UPLOADTYPE_DOCUMENT,     /*!< 文档类型 (txt, doc, docx, pdf, etc.) */
    LUSP_UPLOADTYPE_IMAGE,        /*!< 图片类型 (jpg, png, gif, bmp, etc.) */
    LUSP_UPLOADTYPE_VIDEO,        /*!< 视频类型 (mp4, avi, mkv, mov, etc.) */
    LUSP_UPLOADTYPE_AUDIO,        /*!< 音频类型 (mp3, wav, flac, aac, etc.) */
    LUSP_UPLOADTYPE_ARCHIVE,      /*!< 压缩包类型 (zip, rar, 7z, tar, etc.) */
    LUSP_UPLOADTYPE_CODE,         /*!< 代码文件 (cpp, h, js, py, etc.) */
    LUSP_UPLOADTYPE_UNDEFINED     /*!< 未知类型 */
};

enum class Lusp_FileExistPolicy {
    LUSP_FILE_EXIST_POLICY_OVERWRITE,   /*!< 覆盖已存在的文件 */
    LUSP_FILE_EXIST_POLICY_SKIP,        /*!< 跳过已存在的文件 */
    LUSP_FILE_EXIST_POLICY_RENAME,      /*!< 重命名已存在的文件 */
    LUSP_FILE_EXIST_POLICY_UNDEFINED    /*!< 未定义策略 */
};

// 使用宏定义枚举转换支持
DEFINE_ENUM_SUPPORT(Lusp_UploadStatusInf)
DEFINE_ENUM_SUPPORT(Lusp_UploadFileTyped)
DEFINE_ENUM_SUPPORT(Lusp_FileExistPolicy)

/**
 * @brief 同步上传文件信息结构体
 *
 * 该结构体用于存储文件上传过程中的所有相关信息，包括文件基本信息、
 * 上传配置、状态跟踪等。支持LAN和WAN两种上传模式。
 *
 * @details 结构体包含以下主要功能：
 * - 文件基本信息管理（名称、大小、MD5等）
 * - 上传策略配置（文件存在处理策略等）
 * - 状态跟踪（上传状态、时间戳等）
 * - 设备信息记录（客户端设备名称）
 * - 认证信息存储（WAN模式下的token）
 *
 * @note 该结构体在初始化时会将所有枚举类型设置为UNDEFINED状态，
 *       字符串类型设置为空字符串，数值类型设置为0
 *
 * @see Lusp_UploadFileTyped 文件上传类型枚举
 * @see Lusp_FileExistPolicy 文件存在策略枚举
 * @see Lusp_UploadStatusInf 上传状态信息枚举
 *
 * @author [hesphoros]
 * @date [2025-6-21]
 * @version 1.0
 */
    typedef struct  Lusp_SyncUploadFileInfo {
    Lusp_UploadFileTyped               eUploadFileTyped;            /*!< 文件类型      */
    std::u16string                     sLanClientDevice;            /*!< 客户端设备名称 */
    size_t                             sSyncFileSizeValue;          /*!< 同步文件大小 */
    std::u16string                     sFileFullNameValue;          /*!< 同步全路径名称 */
    std::u16string                     sOnlyFileNameValue;          /*!< 仅仅文件名 */
    std::string                        sFileRecordTimeValue;        /*!< 文件记录时间 */
    std::string                        sFileMd5ValueInfo;           /*!< 文件MD5值 */
    Lusp_FileExistPolicy               eFileExistPolicy;            /*!< 文件存在策略 */
    std::string                        sAuthTokenValues;            /*!< Wan 上传时的tokenkey LAN 中不使用 */
    uint64_t                           uUploadTimeStamp;            /*!< 上传时间戳  精确级别：毫秒级*/
    Lusp_UploadStatusInf               eUploadStatusInf;            /*!< 上传状态 */
    std::u16string                     sDescriptionInfo;            /*!< 描述信息 在没有上传成功时被赋值*/
    std::chrono::steady_clock::time_point   enqueueTime;              /*!< 入队时间戳（用于队列延迟统计）*/

}Lusp_SyncUploadFileInfo, * PLusp_SyncUploadFileInfo;


class Lusp_SyncUploadFileInfoHandler {
public:
    Lusp_SyncUploadFileInfoHandler() = delete;
    explicit   Lusp_SyncUploadFileInfoHandler(const std::u16string& filePath);
    virtual   ~Lusp_SyncUploadFileInfoHandler();

    // 基本信息获取
    bool                         isValid()                const { return m_valid; }
    std::string                  getError()               const { return m_error; }
    const Lusp_SyncUploadFileInfo& getFileInfo()          const { return m_fileInfo; }
    const Lusp_SyncUploadFileInfo& getFileInfoStruct()    const { return m_fileInfo; }
    size_t                       getFileSize()            const { return m_fileInfo.sSyncFileSizeValue; }

    // 文件路径/名称/设备
    std::string                  getFilePath()            const { return LUSP_UNICONV->ToLocaleFromUtf16LE(m_fileInfo.sFileFullNameValue); }
    std::string                  getFileName()            const { return LUSP_UNICONV->ToLocaleFromUtf16LE(m_fileInfo.sOnlyFileNameValue); }
    std::string                  getClientDevice()        const { return LUSP_UNICONV->ToLocaleFromUtf16LE(m_fileInfo.sLanClientDevice); }
    std::u16string               getFilePathU16()         const { return m_fileInfo.sFileFullNameValue; }
    std::u16string               getFileNameU16()         const { return m_fileInfo.sOnlyFileNameValue; }
    std::u16string               getClientDeviceU16()     const { return m_fileInfo.sLanClientDevice; }

    // 文件描述/类型/策略
    std::string                  getDescription()         const { return LUSP_UNICONV->ToLocaleFromUtf16LE(m_fileInfo.sDescriptionInfo); }
    std::u16string               getDescriptionU16()      const { return m_fileInfo.sDescriptionInfo; }
    std::string                  getFileTypeText()        const;
    std::u16string               getFileTypeTextU16()     const;
    std::string                  getFileExistPolicyText() const;

    // 状态/进度/时间
    std::string                  getStatusText()          const;
    int                          getProgressPercentage()  const;
    std::string                  getFormatUploadTimestamp() const;
    uint64_t                     getUploadTimeStamp()     const { return m_fileInfo.uUploadTimeStamp; }
    std::string                  getCurrentTimeString()   const;

    // 文件MD5/唯一标识
    bool                         calculateFileMd5ValueInfo();
    std::string                  getMd5Hash()             const { return m_fileInfo.sFileMd5ValueInfo; }
    std::string                  getId()                  const { return m_id; }

    // 设备信息
    std::string                  getComputerName();
    std::u16string               getComputerNameU16();

    // 设置相关
    void                         setCurrentTimestampMs();
    void                         setFileName(const std::string& name) { m_fileInfo.sOnlyFileNameValue = LUSP_UNICONV->ToUtf16LEFromUtf8(name); }
    void                         setFileName(const std::u16string& name);
    void                         setFileInfoPath(const std::string& filePath);
    void                         setFileInfoPath(const std::u16string& filePath); // 只声明不实现
    void                         setFileSize(size_t size) { m_fileInfo.sSyncFileSizeValue = size; }
    void                         setMd5Hash(const std::string& md5) { m_fileInfo.sFileMd5ValueInfo = md5; }
    void                         setClientDevice(const std::string& device) { m_fileInfo.sLanClientDevice = LUSP_UNICONV->ToUtf16LEFromUtf8(device); }
    void                         setClientDevice(const std::u16string& device);
    void                         setUploadTimeStamp(uint64_t timestamp) { m_fileInfo.uUploadTimeStamp = timestamp; }
    void                         setDescription(const std::string& desc) { m_fileInfo.sDescriptionInfo = LUSP_UNICONV->ToUtf16LEFromUtf8(desc); }
    void                         setDescription(const std::u16string& desc);
    void                         setRecordTime(const std::string& recordTime) { m_fileInfo.sFileRecordTimeValue = recordTime; }
    void                         setFileExistPolicy(Lusp_FileExistPolicy policy) { m_fileInfo.eFileExistPolicy = policy; }
    void                         setId(const std::string& id) { m_id = id; }

private:
    void                         updateFileInfoFromFileSystem();
    std::string                  generateUuidWindows();
    void                         initializeDefaults();
    Lusp_UploadFileTyped         detectFileType(const std::string& filePath) const;

    Lusp_SyncUploadFileInfo      m_fileInfo;
    std::string                  m_id;
    uint64_t                     m_uploadedBytesCount;
    bool                         m_valid = false;
    std::string                  m_error;
};



#endif // LUSP_SYNC_UPLOAD_FILE_INFO_H
