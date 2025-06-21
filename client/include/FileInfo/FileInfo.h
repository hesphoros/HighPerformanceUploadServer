#pragma once

#include <QString>
#include <QDateTime>
#include <QFileInfo>
#include "md5.h"
#include <windows.h>
#include <iostream>

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
    uint64_t                           uUploadTimeStamp;            /*!< 上传时间戳 */
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

// 注释掉main1函数，避免多重定义错误
/*
int main1() {
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);

    if (GetComputerNameA(computerName, &size)) {
        std::cout << "Computer Name: " << computerName << std::endl;
    } else {
        std::cerr << "Failed to get computer name. Error: " << GetLastError() << std::endl;
    }

    return 0;
}
*/
