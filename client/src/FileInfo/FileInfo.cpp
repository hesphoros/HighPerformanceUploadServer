#include "FileInfo/FileInfo.h"
#include "log_headers.h"
#include "UniConv.h"
#include <codecvt>

#pragma comment(lib, "rpcrt4.lib")



std::string Lusp_SyncUploadFileInfoHandler::getComputerName() {
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
        return std::string(computerName);
    }
    else {
        return "Unknown-ComputerName"; // 获取计算机失败返回默认值
    }
}

std::u16string Lusp_SyncUploadFileInfoHandler::getComputerNameU16() {
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
        return UniConv::GetInstance()->ToUtf16LEFromLocale(computerName);
    }
    else {
        return u"Unknown-ComputerName"; // 获取计算机失败返回默认值
    }
}



void Lusp_SyncUploadFileInfoHandler::initializeDefaults() {
    m_id = m_fileInfo.sAuthTokenValues = generateUuidWindows();
    m_fileInfo.eUploadFileTyped = Lusp_UploadFileTyped::LUSP_UPLOADTYPE_UNDEFINED;
    m_fileInfo.sLanClientDevice = this->getComputerNameU16();
    m_fileInfo.sSyncFileSizeValue = 0;
    m_fileInfo.eFileExistPolicy = Lusp_FileExistPolicy::LUSP_FILE_EXIST_POLICY_OVERWRITE;
    m_fileInfo.uUploadTimeStamp = 0;
    m_fileInfo.eUploadStatusInf = Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_PENDING;
    m_fileInfo.sFileRecordTimeValue = {};
}


Lusp_SyncUploadFileInfoHandler::~Lusp_SyncUploadFileInfoHandler() {
}


std::string Lusp_SyncUploadFileInfoHandler::getCurrentTimeString() const {
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm* timeInfo = std::localtime(&now);
    if (!timeInfo) return std::string();
    std::ostringstream oss;
    oss << std::put_time(timeInfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Lusp_SyncUploadFileInfoHandler::setFileInfoPath(const std::u16string& filePathU16) {
    // 将 UTF-16LE 转为 UTF-8 以便 std::filesystem::u8path 正确解析
    std::string filePathUtf8 = UniConv::GetInstance()->ToUtf8FromUtf16LE(filePathU16);
    std::filesystem::path fsPath = std::filesystem::u8path(filePathUtf8);
    m_fileInfo.sFileFullNameValue = filePathU16;
    m_fileInfo.sOnlyFileNameValue = UniConv::GetInstance()->ToUtf16LEFromLocale(fsPath.filename().u8string());
    g_luspLogWriteImpl.WriteLogContent(
        LOG_DEBUG,
        "设置文件路径: " + filePathUtf8 + " (文件名: " + fsPath.filename().u8string() + ")"
    );
    m_fileInfo.eUploadFileTyped = this->detectFileType(filePathUtf8);
}

void Lusp_SyncUploadFileInfoHandler::setFileInfoPath(const std::string& filePath) {
    setFileInfoPath(UniConv::GetInstance()->ToUtf16LEFromLocale(filePath));
}

Lusp_UploadFileTyped Lusp_SyncUploadFileInfoHandler::detectFileType(const std::string& filePath) const {
    std::filesystem::path path(filePath);
    std::string suffix = path.extension().string(); // e.g ".jpg"
    if (!suffix.empty() && suffix[0] == '.')
        suffix.erase(0, 1); // remove leading dot
    std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower); // 转为小写

    if (!path.has_extension()) {
        return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_UNDEFINED; // 无扩展名，返回未定义类型
    }

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


    return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_UNDEFINED; // 默认返回未定义类型
}

void Lusp_SyncUploadFileInfoHandler::updateFileInfoFromFileSystem() {
    // 将 UTF-16LE 转为 UTF-8 以便 std::filesystem::u8path 正确解析中文路径
    std::string filePathUtf8 = UniConv::GetInstance()->ToUtf8FromUtf16LE(m_fileInfo.sFileFullNameValue);
    if (filePathUtf8.empty()) {
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "文件路径为空，无法更新文件信息");
        m_valid = false;
        m_error = "文件路径为空，无法更新文件信息";
        return;
    }
    std::filesystem::path path = std::filesystem::u8path(filePathUtf8);
    if (!std::filesystem::exists(path)) {
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "文件不存在: " + filePathUtf8);
        m_valid = false;
        m_error = "文件不存在: " + filePathUtf8;
        return;
    }
    setFileSize(std::filesystem::file_size(path));
    // u8string() 返回 UTF-8 编码，应使用 ToUtf16LEFromUtf8 转换
    setFileName(UniConv::GetInstance()->ToUtf16LEFromUtf8(path.filename().u8string()));
    setRecordTime(getCurrentTimeString());
    if (getMd5Hash().empty()) {
        if (!calculateFileMd5ValueInfo()) {
            g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "计算MD5值失败: " + filePathUtf8);
        }
    }
}

//Lusp_SyncUploadFileInfoHandler::Lusp_SyncUploadFileInfoHandler(const std::string &filePath)
//    : m_uploadedBytesCount(0), m_valid(false)
//{
//    initializeDefaults();
//    if (filePath.empty() || !std::filesystem::exists(filePath)) {
//        m_error = "文件路径无效或文件不存在";
//        return;
//    }
//    this->setFileInfoPath(filePath);
//    m_fileInfo.eUploadFileTyped = this->detectFileType(filePath);
//    updateFileInfoFromFileSystem();
//    m_valid = true;
//}

Lusp_SyncUploadFileInfoHandler::Lusp_SyncUploadFileInfoHandler(const std::u16string& filePath)
    : m_uploadedBytesCount(0), m_valid(false) {
    initializeDefaults();
    if (filePath.empty() || !std::filesystem::exists(filePath)) {
        m_error = "文件路径无效或文件不存在";
        return;
    }
    this->setFileInfoPath(filePath);
    m_fileInfo.eUploadFileTyped = this->detectFileType(UniConv::GetInstance()->ToUtf8FromUtf16LE(filePath));
    updateFileInfoFromFileSystem();
    m_valid = true;
}


std::string Lusp_SyncUploadFileInfoHandler::getFormatUploadTimestamp() const {
    std::time_t uploadTime = m_fileInfo.uUploadTimeStamp / 1000; // 转换为秒
    std::tm* timeInfo = std::localtime(&uploadTime);
    if (!timeInfo) return std::string();
    std::ostringstream oss;
    oss << std::put_time(timeInfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Lusp_SyncUploadFileInfoHandler::setCurrentTimestampMs() {
    auto now = std::chrono::system_clock::now();
    m_fileInfo.uUploadTimeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

std::string Lusp_SyncUploadFileInfoHandler::generateUuidWindows() {
    UUID uuid;
    UuidCreate(&uuid);
    RPC_CSTR str = nullptr;
    std::string uuidStr;
    if (UuidToStringA(&uuid, &str) == RPC_S_OK && str != nullptr) {
        uuidStr = reinterpret_cast<const char*>(str);
        RpcStringFreeA(&str);
    }
    return uuidStr;
}


bool Lusp_SyncUploadFileInfoHandler::calculateFileMd5ValueInfo() {
    if (m_fileInfo.sFileFullNameValue.empty()) {
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "文件路径为空,无法计算MD5值");
        return false;
    }
    // 直接用 std::filesystem::path 支持 utf16/中文路径
    std::filesystem::path fsPath(m_fileInfo.sFileFullNameValue);
    std::ifstream file(fsPath, std::ios::binary);
    if (!file.is_open()) {
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "无法打开文件: " + fsPath.u8string());
        return false;
    }
    std::vector<char> buffer(8192);
    MD5 md5;
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        md5.add(buffer.data(), file.gcount());
    }
    file.close();
    this->setMd5Hash(md5.getHash());
    g_luspLogWriteImpl.WriteLogContent(
        LOG_DEBUG,
        "计算文件MD5值完成: " + fsPath.u8string() + " MD5: " + m_fileInfo.sFileMd5ValueInfo
    );
    return true;
}


std::string Lusp_SyncUploadFileInfoHandler::getStatusText() const {
    switch (m_fileInfo.eUploadStatusInf) {
    case Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_COMPLETED:
        return "COMPLETED";
    case Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_PENDING:
        return "PENDING";
    case Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_UPLOADING:
        return "UPLOADING";
    case Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_REJECTED:
        return "REJECTED";
    case Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_FAILED:
        return "FAILED";
    default:
        return "UNDEFINED";
    }
}

std::string Lusp_SyncUploadFileInfoHandler::getFileTypeText() const {
    switch (m_fileInfo.eUploadFileTyped) {
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_DOCUMENT:
        return "DOCUMENT";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_IMAGE:
        return "IMAGE";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_VIDEO:
        return "VIDEO";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_AUDIO:
        return "AUDIO";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_ARCHIVE:
        return "ARCHIVE";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_CODE:
        return "CODE";
    default:
        return "UNDEFINED";
    }
}


std::u16string Lusp_SyncUploadFileInfoHandler::getFileTypeTextU16() const {
    switch (m_fileInfo.eUploadFileTyped) {
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_DOCUMENT:
        return u"DOCUMENT";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_IMAGE:
        return u"IMAGE";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_VIDEO:
        return u"VIDEO";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_AUDIO:
        return u"AUDIO";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_ARCHIVE:
        return u"ARCHIVE";
    case Lusp_UploadFileTyped::LUSP_UPLOADTYPE_CODE:
        return u"CODE";
    default:
        return u"UNDEFINED";
    }
}


std::string Lusp_SyncUploadFileInfoHandler::getFileExistPolicyText() const {
    switch (m_fileInfo.eFileExistPolicy) {
    case Lusp_FileExistPolicy::LUSP_FILE_EXIST_POLICY_OVERWRITE:
        return "OVERWRITE";
    case Lusp_FileExistPolicy::LUSP_FILE_EXIST_POLICY_SKIP:
        return "SKIP";
    case Lusp_FileExistPolicy::LUSP_FILE_EXIST_POLICY_RENAME:
        return "RENAME";
    default:
        return "UNDEFINED";
    }
}


int Lusp_SyncUploadFileInfoHandler::getProgressPercentage() const {
    if (m_fileInfo.sSyncFileSizeValue == 0) {
        return 0; // 文件大小为0，进度为0%
    }
    return static_cast<int>((static_cast<double>(m_uploadedBytesCount) / m_fileInfo.sSyncFileSizeValue) * 100);
}

void Lusp_SyncUploadFileInfoHandler::setFileName(const std::u16string& name) {
    m_fileInfo.sOnlyFileNameValue = name;
}

void Lusp_SyncUploadFileInfoHandler::setClientDevice(const std::u16string& device) {
    m_fileInfo.sLanClientDevice = device;
}

void Lusp_SyncUploadFileInfoHandler::setDescription(const std::u16string& desc) {
    m_fileInfo.sDescriptionInfo = desc;
}
