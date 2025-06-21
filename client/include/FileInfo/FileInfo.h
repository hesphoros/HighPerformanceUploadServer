#pragma once

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


/**
 * @brief 文件上传状态
 * @enum Lusp_UploadStatusInf
 */
enum class Lusp_UploadStatusInf {
    LUSP_UPLOAD_STATUS_IDENTIFIERS_COMPLETED = 0,  /*!< 成功 */
    LUSP_UPLOAD_STATUS_IDENTIFIERS_PENDING   = 1,  /*!< 等待上传 */
    LUSP_UPLOAD_STATUS_IDENTIFIERS_UPLOADING = 2,  /*!< 上传部分 */
    LUSP_UPLOAD_STATUS_IDENTIFIERS_REJECTED  = 3,  /*!< 拒绝上传 */
    LUSP_UPLOAD_STATUS_IDENTIFIERS_FAILED    = 4,  /*!< 失败 */
    LUSP_UPLOAD_STATUS_IDENTIFIERS_UNDEFINED = 5   /*!< 未定义 */
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
    std::string                        sLanClientDevice;            /*!< 客户端设备名称 */
    size_t                             sSyncFileSizeValue;          /*!< 同步文件大小 */
    std::string                        sFileFullNameValue;          /*!< 同步全路径名称 */
    std::string                        sOnlyFileNameValue;          /*!< 仅仅文件名 */
    std::string                        sFileRecordTimeValue;        /*!< 文件记录时间 */
    std::string                        sFileMd5ValueInfo;           /*!< 文件MD5值 */
    Lusp_FileExistPolicy               eFileExistPolicy;            /*!< 文件存在策略 */
    std::string                        sAuthTokenValues;            /** Wan 上传时的tokenkey LAN 中不使用 */
    uint64_t                           uUploadTimeStamp;            /*!< 上传时间戳  精确级别：毫秒级*/
    Lusp_UploadStatusInf               eUploadStatusInf;            /*!< 上传状态 */
    std::string                        sDescriptionInfo;            /*!< 描述信息 在没有上传成功时被赋值*/

    Lusp_SyncUploadFileInfo() 
        : eUploadFileTyped(Lusp_UploadFileTyped::LUSP_UPLOADTYPE_UNDEFINED),
          sLanClientDevice(""),
          sSyncFileSizeValue(0),
          sFileFullNameValue(""),
          sOnlyFileNameValue(""),
          sFileRecordTimeValue(""),
          sFileMd5ValueInfo(""),
          eFileExistPolicy(Lusp_FileExistPolicy::LUSP_FILE_EXIST_POLICY_UNDEFINED),
          sAuthTokenValues(""),
          uUploadTimeStamp(0),
          eUploadStatusInf(Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_UNDEFINED) {

    }

}Lusp_SyncUploadFileInfo,* PLusp_SyncUploadFileInfo;


class Lusp_SyncUploadFileInfoHandler {
    public:
        Lusp_SyncUploadFileInfoHandler();
        virtual ~Lusp_SyncUploadFileInfoHandler();

        /**
         * @brief 构造函数
         * @param fileInfo 文件信息结构体
         * @details 该构造函数用于初始化文件上传信息处理器。
         */
        Lusp_SyncUploadFileInfoHandler(const Lusp_SyncUploadFileInfo&);

        explicit Lusp_SyncUploadFileInfoHandler(const std::string& filePath);

    public:
        std::string getId() const { return m_id; }  /*!< 获取文件ID */
        std::string getFilePath() const { return m_fileInfo.sFileFullNameValue; }  /*!< 获取文件路径 */
        std::string getFileName() const { return m_fileInfo.sOnlyFileNameValue; }  /*!< 获取文件名 */
        size_t getFileSize() const { return m_fileInfo.sSyncFileSizeValue; }  /*!< 获取文件大小 */
        std::string getMd5Hash() const { return m_fileInfo.sFileMd5ValueInfo; }  /*!< 获取文件MD5值 */
        std::string getClientDevice() const { return m_fileInfo.sLanClientDevice; }  /*!< 获取客户端设备名称 */
        uint64_t getUploadTimeStamp() const { return m_fileInfo.uUploadTimeStamp; }  /*!< 获取上传时间戳 */
        std::string getDescription() const { return m_fileInfo.sDescriptionInfo; }  /*!< 获取描述信息 */
    public:
    //Utility functions for file info management

        /**
         * @brief 获取文件信息
         * @return 返回文件信息结构体
         * @details 该函数用于获取当前文件上传信息的结构体。
         */
        std::string       getStatusText() const; /*!< 获取上传状态文本 */

        /**
         * @brief Get the File Type Text object
         * @details 获取文件类型的文本描述。
         * 
         * @return std::string 
         */
        std::string       getFileTypeText() const; 


        /**
         * @brief 获取文件存在策略文本
         * @details 获取文件存在策略的文本描述。
         * 
         * @return std::string 
         */
        std::string       getFileExistPolicyText() const;


        int               getProgressPercentage() const; /*!< 获取上传进度百分比 */
        /**
         * @brief 获取文件信息
         * @return 返回文件信息结构体
         * * @details 该函数用于获取当前文件上传时间戳的格式化字符串。
         */
        std::string getFormatUploadTimestamp();


        /**
         * @brief calculateMd5
         * @details 计算文件的MD5值。
         * * @note 该函数会读取文件内容并计算其MD5值，结果存储在sFileMd5ValueInfo成员变量中。
         * * @note 如果文件不存在或无法读取，将不会进行计算。
         * @return 是否成功计算MD5值 0 失败 1 成功
         * @retval true 成功计算MD5值
         */
        bool        calculateFileMd5ValueInfo(); /*!< 计算文件MD5值 */

    public:
        /**
         * @brief 更新文件信息 (设置了文件路径后)
         * @details 更新 文件大小 文件名 记录时间等信息。
         * @note 该函数会检查文件是否存在，如果不存在则不进行更新。
         */
        void  updateFileInfoFromFileSystem(); /*!< 更新文件信息 */

    public:
    /**Setting area */
        /**
         * @brief Set the File ID object
         * @param id 文件ID
         * @details 设置文件的唯一标识符。
         * @note 该ID通常用于在上传过程中跟踪文件。 不需要调用 默认使用了UUID
         */
        void         setId(const std::string& id) { m_id = id; }  /*!< 设置文件ID */
        /**
         * @brief Set the File Info Path object
         * 
         * @param filePath  文件路径
         * @details 设置文件信息的路径和名称。
         */
        void         setFileInfoPath(const std::string& filePath);
        void         setFileName(const std::string& name) { m_fileInfo.sOnlyFileNameValue = name; }  /*!< 设置文件名 */
        void         setFileSize(size_t size) { m_fileInfo.sSyncFileSizeValue = size; }  /*!< 设置文件大小 */
        void         setMd5Hash(const std::string& md5) { m_fileInfo.sFileMd5ValueInfo = md5; }  /*!< 设置文件MD5值 */
        void         setClientDevice(const std::string& device) { m_fileInfo.sLanClientDevice = device; }  /*!< 设置客户端设备名称 */
        void         setUploadTimeStamp(uint64_t timestamp) { m_fileInfo.uUploadTimeStamp = timestamp; }  /*!< 设置上传时间戳 */
        void         setDescription(const std::string& desc) { m_fileInfo.sDescriptionInfo = desc; }  /*!< 设置描述信息 */
        void         setRecordTime(const std::string& recordTime) { m_fileInfo.sFileRecordTimeValue = recordTime; }  /*!< 设置记录时间 */
        void         setFileExistPolicy(Lusp_FileExistPolicy policy) { m_fileInfo.eFileExistPolicy = policy; }  /*!< 设置文件存在策略 */
    private:

        /**
         * @brief Get the Current Timestamp Ms object
         * @return 当前时间戳，单位为毫秒
         * * @details 该函数用于获取当前系统时间的时间戳，精确到毫秒级别。
         * @note 设置uUploadTimeStamp成员变量为当前时间戳。
         */
        void  setCurrentTimestampMs();

        std::string   generateUuidWindows();     /*!< 生成UUID */
        void          initializeDefaults();             /*!< 初始化默认值 */
        std::string   getComputerName();         /*!< 获取计算机名称 */

        /**
         * @brief Get the Current Time String object
         * @example "output 2025-06-22 01:08:45"
         * @details 获取当前时间的字符串表示，格式为"YYYY-MM-DD HH:MM:SS"。
         * @return std::string 
         */
        std::string  getCurrentTimeString() const ;    /*!< 获取当前时间字符串 */


        /**
         * @brief 检测文件类型
         * @param filePath 文件路径
         * @return 返回文件类型枚举值
         * @details 根据文件扩展名检测文件类型。
         */
        Lusp_UploadFileTyped detectFileType(const std::string& filePath) const;

    private:
        Lusp_SyncUploadFileInfo     m_fileInfo;    /*!< 文件信息 */
        std::string                       m_id;    /*!< 文件ID */
        uint64_t          m_uploadedBytesCount;    /*!< 已上传字节数 */
    
};


