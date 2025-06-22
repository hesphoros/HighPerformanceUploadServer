#include "FileInfo/FileInfo.h"
#include "log_headers.h"

#pragma comment(lib, "rpcrt4.lib")


std::string Lusp_SyncUploadFileInfoHandler::getComputerName()
{
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
        return std::string(computerName);
    } else {
        return "Unknown-ComputerName"; // 获取计算机失败返回默认值
    }
}



void Lusp_SyncUploadFileInfoHandler::initializeDefaults()
{
    m_id = generateUuidWindows(); // 生成唯一ID
    m_fileInfo.eUploadFileTyped = Lusp_UploadFileTyped::LUSP_UPLOADTYPE_UNDEFINED;
    m_fileInfo.sLanClientDevice = this->getComputerName();
    m_fileInfo.sSyncFileSizeValue = 0;
    m_fileInfo.eFileExistPolicy = Lusp_FileExistPolicy::LUSP_FILE_EXIST_POLICY_OVERWRITE; // 默认覆盖策略
    m_fileInfo.uUploadTimeStamp = 0; // 初始时间戳为0
    m_fileInfo.eUploadStatusInf = Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_PENDING;
    m_fileInfo.sFileRecordTimeValue = {}; // 初始记录时间为空
}


Lusp_SyncUploadFileInfoHandler::~Lusp_SyncUploadFileInfoHandler()
{
}


std::string Lusp_SyncUploadFileInfoHandler::getCurrentTimeString() const
{
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm* timeInfo = std::localtime(&now);
    if (!timeInfo) return std::string();
    std::ostringstream oss;
    oss << std::put_time(timeInfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Lusp_SyncUploadFileInfoHandler::setFileInfoPath(const std::string &filePath)
{
    // 使用 std::filesystem 处理路径，自动适配分隔符，无需手动替换
    std::filesystem::path fsPath = std::filesystem::u8path(filePath);
    m_fileInfo.sFileFullNameValue = fsPath.u8string(); // 保持原始格式
    m_fileInfo.sOnlyFileNameValue = fsPath.filename().u8string(); // 获取文件名
    g_luspLogWriteImpl.WriteLogContent(
        LOG_DEBUG,
        "设置文件路径: " + m_fileInfo.sFileFullNameValue + " (文件名: " + m_fileInfo.sOnlyFileNameValue + ")"
    );
    m_fileInfo.eUploadFileTyped = this->detectFileType(m_fileInfo.sFileFullNameValue); // 检测文件类型
}

Lusp_UploadFileTyped Lusp_SyncUploadFileInfoHandler::detectFileType(const std::string &filePath) const
{
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

void Lusp_SyncUploadFileInfoHandler::updateFileInfoFromFileSystem()
{
    const std::string &filePath = this->getFilePath();
    if (filePath.empty()) {
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "文件路径为空，无法更新文件信息");
        m_valid = false;
        m_error = "文件路径为空，无法更新文件信息";
        return;
    }
    std::filesystem::path path(filePath);
    if (!std::filesystem::exists(path)) {
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "文件不存在: " + filePath);
        m_valid = false;
        m_error = "文件不存在: " + filePath;
        return; // 文件不存在，无法更新信息
    }
    setFileSize(std::filesystem::file_size(path));
    // 更新文件名（获取最后一个路径组件）（可能路径中的文件名与实际文件名不同）
    setFileName(path.filename().string());
    // 更新记录时间
    setRecordTime(getCurrentTimeString()); // 使用当前时间作为记录时间
    if(getMd5Hash().empty()) {
       // 计算md5
         if (!calculateFileMd5ValueInfo()) {
              g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "计算MD5值失败: " + filePath);
         }
    }
}

Lusp_SyncUploadFileInfoHandler::Lusp_SyncUploadFileInfoHandler(const std::string &filePath)
    : m_uploadedBytesCount(0), m_valid(false)
{
    initializeDefaults();
    if (filePath.empty() || !std::filesystem::exists(filePath)) {
        m_error = "文件路径无效或文件不存在";
        return;
    }
    this->setFileInfoPath(filePath);
    m_fileInfo.eUploadFileTyped = this->detectFileType(filePath);
    updateFileInfoFromFileSystem();
    // 可扩展更多字段校验
    m_valid = true;
}

std::string Lusp_SyncUploadFileInfoHandler::getFormatUploadTimestamp() const
{
    std::time_t uploadTime = m_fileInfo.uUploadTimeStamp / 1000; // 转换为秒
    std::tm* timeInfo = std::localtime(&uploadTime);
    if (!timeInfo) return std::string();
    std::ostringstream oss;
    oss << std::put_time(timeInfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Lusp_SyncUploadFileInfoHandler::setCurrentTimestampMs()
{
    auto now = std::chrono::system_clock::now();
    m_fileInfo.uUploadTimeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

std::string Lusp_SyncUploadFileInfoHandler::generateUuidWindows()
{
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


bool Lusp_SyncUploadFileInfoHandler::calculateFileMd5ValueInfo()
{
    if (m_fileInfo.sFileFullNameValue.empty()) {
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "文件路径为空,无法计算MD5值");
        return false;
    }
    std::ifstream file(m_fileInfo.sFileFullNameValue, std::ios::binary);
    if (!file.is_open()) {
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "无法打开文件: " + m_fileInfo.sFileFullNameValue);
        return false;
    }

    std::vector<char> buffer(8192); // 修正为char类型，兼容ifstream::read
    MD5 md5;

    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        md5.add(buffer.data(), file.gcount());
    }

    file.close();
    this->setMd5Hash(md5.getHash());
    g_luspLogWriteImpl.WriteLogContent(
        LOG_DEBUG,
        "计算文件MD5值完成: " + m_fileInfo.sFileFullNameValue + " MD5: " + m_fileInfo.sFileMd5ValueInfo
    );
    return true;
}


std::string Lusp_SyncUploadFileInfoHandler::getStatusText() const
{
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

std::string Lusp_SyncUploadFileInfoHandler::getFileTypeText() const
{
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

std::string Lusp_SyncUploadFileInfoHandler::getFileExistPolicyText() const
{
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


int Lusp_SyncUploadFileInfoHandler::getProgressPercentage() const
{
    if (m_fileInfo.sSyncFileSizeValue == 0) {
        return 0; // 文件大小为0，进度为0%
    }
    return static_cast<int>((static_cast<double>(m_uploadedBytesCount) / m_fileInfo.sSyncFileSizeValue) * 100);
}