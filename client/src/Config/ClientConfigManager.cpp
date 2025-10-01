#include "Config/ClientConfigManager.h"
#include "log_headers.h"
#include <mutex>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>
#include "utils/EnumConvert.hpp"

// 不再使用 using namespace utils，以避免命名冲突

using namespace utils;
// TOML支持 - 使用项目内置toml11库
#include <toml11/toml.hpp>

// =============================================================================
// EnumConvert转换器已在头文件中定义，无需重复定义
// ============================================================================= 

// ===================== 简化的TOML文件操作方法 =====================

bool ClientConfigManager::loadFromFile(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(m_configMutex);

    try {
        if (!std::filesystem::exists(configPath)) {
            m_lastError = "配置文件不存在: " + configPath;
            g_luspLogWriteImpl.WriteLogContent(LOG_WARN, m_lastError);
            return false;
        }

        std::ifstream file(configPath);
        if (!file.is_open()) {
            m_lastError = "无法打开配置文件: " + configPath;
            g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, m_lastError);
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();

        if (parseFullTomlConfig(content)) {
            m_currentConfigPath = configPath;
            g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "成功加载配置文件: " + configPath);
            return true;
        }
        return false;

    }
    catch (const std::exception& e) {
        m_lastError = "加载配置文件异常: " + std::string(e.what());
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, m_lastError);
        return false;
    }
}

bool ClientConfigManager::saveToFile(const std::string& configPath) const {
    std::lock_guard<std::mutex> lock(m_configMutex);

    try {
        // 确保目录存在
        std::filesystem::path filePath(configPath);
        if (filePath.has_parent_path()) {
            std::filesystem::create_directories(filePath.parent_path());
        }

        std::ofstream file(configPath);
        if (!file.is_open()) {
            g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "无法创建配置文件: " + configPath);
            return false;
        }

        std::string tomlContent = generateFullTomlConfig();
        file << tomlContent;
        file.close();

        g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "成功保存配置文件: " + configPath);
        return true;

    }
    catch (const std::exception& e) {
        std::string error = "保存配置文件异常: " + std::string(e.what());
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, error);
        return false;
    }
}

// ===================== TOML辅助方法实现 =====================

template<typename T>
T ClientConfigManager::getTomlValue(const toml::value& table, const std::string& key, const T& defaultValue) const {
    try {
        if (table.contains(key)) {
            return toml::get<T>(table.at(key));
        }
        return defaultValue;
    }
    catch (const std::exception& e) {
        g_luspLogWriteImpl.WriteLogContent(LOG_WARN,
            "获取TOML值失败 [" + key + "]: " + e.what() + "，使用默认值");
        return defaultValue;
    }
}

// 模板特化实例化
template std::string ClientConfigManager::getTomlValue<std::string>(const toml::value&, const std::string&, const std::string&) const;
template int ClientConfigManager::getTomlValue<int>(const toml::value&, const std::string&, const int&) const;
template uint32_t ClientConfigManager::getTomlValue<uint32_t>(const toml::value&, const std::string&, const uint32_t&) const;
template uint16_t ClientConfigManager::getTomlValue<uint16_t>(const toml::value&, const std::string&, const uint16_t&) const;
template uint64_t ClientConfigManager::getTomlValue<uint64_t>(const toml::value&, const std::string&, const uint64_t&) const;
template bool ClientConfigManager::getTomlValue<bool>(const toml::value&, const std::string&, const bool&) const;

// 静态成员初始化
std::unique_ptr<ClientConfigManager> ClientConfigManager::m_instance = nullptr;
std::mutex ClientConfigManager::m_instanceMutex;

ClientConfigManager& ClientConfigManager::getInstance() {
    std::lock_guard<std::mutex> lock(m_instanceMutex);
    if (!m_instance) {
        m_instance = std::unique_ptr<ClientConfigManager>(new ClientConfigManager());
    }
    return *m_instance;
}

ClientConfigManager::ClientConfigManager() {
    initializeDefaults();
}

ClientConfigManager::~ClientConfigManager() {
    // 析构函数实现
}

void ClientConfigManager::initializeDefaults() {
    // 上传配置默认值已在结构体中设置
    // 但需要特别设置枚举类型的默认值
    m_uploadConfig = UploadConfig{};
    m_uploadConfig.compressionAlgo = CompressionAlgorithm::ZSTD;
    m_uploadConfig.checksumAlgo = ChecksumAlgorithm::SHA256;

    // UI配置默认值已在结构体中设置
    m_uiConfig = UIConfig{};

    // 网络配置默认值已在结构体中设置
    m_networkConfig = NetworkConfig{};

    // 清空错误信息
    m_lastError.clear();
    m_currentConfigPath.clear();

    // 网络配置默认值已在结构体中设置

    g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "ClientConfigManager: 配置管理器已初始化为默认值");
}



void ClientConfigManager::setDefaults() {
    initializeDefaults();
    g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "ClientConfigManager: 已重置为默认配置");
}

bool ClientConfigManager::validateConfig() const {
    std::vector<std::string> errors;
    bool isValid = validateUploadConfig(errors) &&
        validateUIConfig(errors) &&
        validateNetworkConfig(errors);

    if (!isValid) {
        std::string errorMsg = "配置验证失败，错误数量: " + std::to_string(errors.size());
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, errorMsg);
    }

    return isValid;
}

std::vector<std::string> ClientConfigManager::getValidationErrors() const {
    std::vector<std::string> errors;
    validateUploadConfig(errors);
    validateUIConfig(errors);
    validateNetworkConfig(errors);
    return errors;
}

void ClientConfigManager::setConfigChangeCallback(ConfigChangeCallback callback) {
    m_changeCallback = std::move(callback);
}

void ClientConfigManager::notifyConfigChanged(const std::string& section, const std::string& key) {
    if (m_changeCallback) {
        m_changeCallback(section, key);
    }
}

std::string ClientConfigManager::getDefaultConfigPath() const {
    // 返回默认的配置文件路径
#ifdef _WIN32
    return "./config/upload_client.json";
#else
    return "~/.config/upload_client/config.json";
#endif
}

bool ClientConfigManager::configFileExists(const std::string& configPath) const {
    return std::filesystem::exists(configPath) && std::filesystem::is_regular_file(configPath);
}

bool ClientConfigManager::createDefaultConfigFile(const std::string& configPath) const {
    try {
        // 确保配置目录存在
        std::filesystem::path filePath(configPath);
        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec);

        if (ec) {
            m_lastError = "无法创建配置目录: " + ec.message();
            g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, m_lastError);
            return false;
        }

        // 创建默认配置内容
        std::ofstream file(configPath);
        if (!file.is_open()) {
            m_lastError = "无法创建配置文件: " + configPath;
            g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, m_lastError);
            return false;
        }

        file << generateFullTomlConfig();
        file.close();

        g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "成功创建默认配置文件: " + configPath);
        return true;

    }
    catch (const std::exception& e) {
        m_lastError = "创建默认配置文件失败: " + std::string(e.what());
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, m_lastError);
        return false;
    }
}

std::string ClientConfigManager::getConfigSummary() const {
    std::ostringstream oss;
    oss << "=== ClientConfigManager 配置摘要 ===\n";
    oss << "上传服务器: " << m_uploadConfig.serverHost << ":" << m_uploadConfig.serverPort << "\n";
    oss << "上传协议: " << m_uploadConfig.uploadProtocol << "\n";
    oss << "最大并发: " << m_uploadConfig.maxConcurrentUploads << "\n";
    oss << "分块大小: " << (m_uploadConfig.chunkSize / 1024) << " KB\n";
    oss << "断点续传: " << (m_uploadConfig.enableResume ? "启用" : "禁用") << "\n";
    oss << "SSL/TLS: " << (m_uploadConfig.useSSL ? "启用" : "禁用") << "\n";
    oss << "日志级别: " << m_uploadConfig.logLevel << "\n";
    oss << "界面语言: " << m_uiConfig.language << "\n";
    oss << "连接超时: " << m_networkConfig.connectTimeoutMs << " ms\n";
    return oss.str();
}

// ===================== 内部验证方法 =====================

bool ClientConfigManager::validateUploadConfig(std::vector<std::string>& errors) const {
    bool isValid = true;

    // 验证服务器地址
    if (m_uploadConfig.serverHost.empty()) {
        errors.push_back("服务器地址不能为空");
        isValid = false;
    }

    // 验证端口号
    if (m_uploadConfig.serverPort == 0 || m_uploadConfig.serverPort > 65535) {
        errors.push_back("服务器端口号无效 (应在1-65535范围内)");
        isValid = false;
    }

    // 验证协议类型
    const std::vector<std::string> supportedProtocols = { "HTTP", "HTTPS", "FTP", "FTPS", "TCP", "UDP", "gRPC", "WebSocket" };
    bool protocolValid = false;
    for (const auto& protocol : supportedProtocols) {
        if (m_uploadConfig.uploadProtocol == protocol) {
            protocolValid = true;
            break;
        }
    }
    if (!protocolValid) {
        errors.push_back("不支持的上传协议: " + m_uploadConfig.uploadProtocol);
        isValid = false;
    }

    // 验证并发数
    if (m_uploadConfig.maxConcurrentUploads == 0 || m_uploadConfig.maxConcurrentUploads > 50) {
        errors.push_back("最大并发上传数应在1-50范围内");
        isValid = false;
    }

    // 验证分块大小
    if (m_uploadConfig.chunkSize < 1024 || m_uploadConfig.chunkSize > 100 * 1024 * 1024) {
        errors.push_back("分块大小应在1KB-100MB范围内");
        isValid = false;
    }

    // 验证超时时间
    if (m_uploadConfig.timeoutSeconds == 0 || m_uploadConfig.timeoutSeconds > 3600) {
        errors.push_back("超时时间应在1-3600秒范围内");
        isValid = false;
    }

    return isValid;
}

bool ClientConfigManager::validateUIConfig(std::vector<std::string>& errors) const {
    bool isValid = true;

    // 验证窗口大小
    if (m_uiConfig.windowWidth < 800 || m_uiConfig.windowWidth > 4000) {
        errors.push_back("窗口宽度应在800-4000像素范围内");
        isValid = false;
    }

    if (m_uiConfig.windowHeight < 600 || m_uiConfig.windowHeight > 3000) {
        errors.push_back("窗口高度应在600-3000像素范围内");
        isValid = false;
    }

    // 验证语言设置
    const std::vector<std::string> supportedLanguages = { "zh-CN", "en-US", "ja-JP" };
    bool languageValid = false;
    for (const auto& lang : supportedLanguages) {
        if (m_uiConfig.language == lang) {
            languageValid = true;
            break;
        }
    }
    if (!languageValid) {
        errors.push_back("不支持的语言: " + m_uiConfig.language);
        isValid = false;
    }

    return isValid;
}

bool ClientConfigManager::validateNetworkConfig(std::vector<std::string>& errors) const {
    bool isValid = true;

    // 验证超时时间
    if (m_networkConfig.connectTimeoutMs < 1000 || m_networkConfig.connectTimeoutMs > 60000) {
        errors.push_back("连接超时时间应在1-60秒范围内");
        isValid = false;
    }

    if (m_networkConfig.readTimeoutMs < 5000 || m_networkConfig.readTimeoutMs > 300000) {
        errors.push_back("读取超时时间应在5-300秒范围内");
        isValid = false;
    }

    // 验证缓冲区大小
    if (m_networkConfig.bufferSize < 1024 || m_networkConfig.bufferSize > 1024 * 1024) {
        errors.push_back("缓冲区大小应在1KB-1MB范围内");
        isValid = false;
    }

    // 验证最大连接数
    if (m_networkConfig.maxConnections == 0 || m_networkConfig.maxConnections > 100) {
        errors.push_back("最大连接数应在1-100范围内");
        isValid = false;
    }

    return isValid;
}

// =============================================================================
// TOML相关方法实现框架
// =============================================================================

std::string ClientConfigManager::exportToTomlString() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return generateFullTomlConfig();
}

bool ClientConfigManager::importFromTomlString(const std::string& tomlContent) {
    std::lock_guard<std::mutex> lock(m_configMutex);

    try {
        if (parseFullTomlConfig(tomlContent)) {
            if (m_changeCallback) {
                m_changeCallback("all", "import_from_string");
            }
            return true;
        }
        return false;
    }
    catch (const std::exception& e) {
        m_lastError = "导入TOML字符串失败: " + std::string(e.what());
        return false;
    }
}

std::string ClientConfigManager::generateFullTomlConfig() const {
    std::ostringstream oss;

    oss << "# 高性能文件上传客户端配置文件" << std::endl;
    oss << "# 格式: TOML (Tom's Obvious, Minimal Language)" << std::endl;
    oss << "# 生成时间: 2025-09-30" << std::endl << std::endl;

    // 上传配置节
    oss << "[upload]" << std::endl;
    oss << "server_host = \"" << m_uploadConfig.serverHost << "\"" << std::endl;
    oss << "server_port = " << m_uploadConfig.serverPort << std::endl;
    oss << "upload_protocol = \"" << m_uploadConfig.uploadProtocol << "\"" << std::endl;
    oss << "max_concurrent_uploads = " << m_uploadConfig.maxConcurrentUploads << std::endl;
    oss << "chunk_size = " << m_uploadConfig.chunkSize << std::endl;
    oss << "timeout_seconds = " << m_uploadConfig.timeoutSeconds << std::endl;
    oss << "retry_count = " << m_uploadConfig.retryCount << std::endl;
    oss << "retry_delay_ms = " << m_uploadConfig.retryDelayMs << std::endl;
    oss << "max_upload_speed = " << m_uploadConfig.maxUploadSpeed << std::endl;
    oss << "max_file_size = " << m_uploadConfig.maxFileSize << std::endl;
    oss << "enable_resume = " << (m_uploadConfig.enableResume ? "true" : "false") << std::endl;
    oss << "enable_compression = " << (m_uploadConfig.enableCompression ? "true" : "false") << std::endl;
    oss << "compression_algorithm = \"" << m_uploadConfig.compressionAlgo << "\"" << std::endl;
    oss << "enable_checksum = " << (m_uploadConfig.enableChecksum ? "true" : "false") << std::endl;
    oss << "checksum_algorithm = \"" << m_uploadConfig.checksumAlgo << "\"" << std::endl;
    oss << "overwrite = " << (m_uploadConfig.overwrite ? "true" : "false") << std::endl;
    oss << "enable_multipart = " << (m_uploadConfig.enableMultipart ? "true" : "false") << std::endl;
    oss << "enable_progress = " << (m_uploadConfig.enableProgress ? "true" : "false") << std::endl;
    oss << "target_dir = \"" << m_uploadConfig.targetDir << "\"" << std::endl;
    oss << "use_ssl = " << (m_uploadConfig.useSSL ? "true" : "false") << std::endl;
    oss << "cert_file = \"" << m_uploadConfig.certFile << "\"" << std::endl;
    oss << "private_key_file = \"" << m_uploadConfig.privateKeyFile << "\"" << std::endl;
    oss << "ca_file = \"" << m_uploadConfig.caFile << "\"" << std::endl;
    oss << "verify_server = " << (m_uploadConfig.verifyServer ? "true" : "false") << std::endl;
    oss << "auth_token = \"" << m_uploadConfig.authToken << "\"" << std::endl;
    oss << "log_level = \"" << m_uploadConfig.logLevel << "\"" << std::endl;
    oss << "log_file_path = \"" << m_uploadConfig.logFilePath << "\"" << std::endl;
    oss << "enable_detailed_log = " << (m_uploadConfig.enableDetailedLog ? "true" : "false") << std::endl;
    oss << "client_version = \"" << m_uploadConfig.clientVersion << "\"" << std::endl;
    oss << "user_agent = \"" << m_uploadConfig.userAgent << "\"" << std::endl;

    // 排除模式数组
    if (!m_uploadConfig.excludePatterns.empty()) {
        oss << "exclude_patterns = [";
        for (size_t i = 0; i < m_uploadConfig.excludePatterns.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << m_uploadConfig.excludePatterns[i] << "\"";
        }
        oss << "]" << std::endl;
    }

    oss << std::endl;

    // UI配置节
    oss << "[ui]" << std::endl;
    oss << "show_progress_details = " << (m_uiConfig.showProgressDetails ? "true" : "false") << std::endl;
    oss << "show_speed_info = " << (m_uiConfig.showSpeedInfo ? "true" : "false") << std::endl;
    oss << "auto_start_upload = " << (m_uiConfig.autoStartUpload ? "true" : "false") << std::endl;
    oss << "minimize_to_tray = " << (m_uiConfig.minimizeToTray ? "true" : "false") << std::endl;
    oss << "show_notifications = " << (m_uiConfig.showNotifications ? "true" : "false") << std::endl;
    oss << "language = \"" << m_uiConfig.language << "\"" << std::endl;
    oss << "theme = \"" << m_uiConfig.theme << "\"" << std::endl;
    oss << "window_width = " << m_uiConfig.windowWidth << std::endl;
    oss << "window_height = " << m_uiConfig.windowHeight << std::endl;
    oss << "window_maximized = " << (m_uiConfig.windowMaximized ? "true" : "false") << std::endl;
    oss << "show_file_size = " << (m_uiConfig.showFileSize ? "true" : "false") << std::endl;
    oss << "show_file_type = " << (m_uiConfig.showFileType ? "true" : "false") << std::endl;
    oss << "show_upload_time = " << (m_uiConfig.showUploadTime ? "true" : "false") << std::endl;
    oss << "show_file_status = " << (m_uiConfig.showFileStatus ? "true" : "false") << std::endl;

    oss << std::endl;

    // 网络配置节
    oss << "[network]" << std::endl;
    oss << "connect_timeout_ms = " << m_networkConfig.connectTimeoutMs << std::endl;
    oss << "read_timeout_ms = " << m_networkConfig.readTimeoutMs << std::endl;
    oss << "write_timeout_ms = " << m_networkConfig.writeTimeoutMs << std::endl;
    oss << "buffer_size = " << m_networkConfig.bufferSize << std::endl;
    oss << "max_connections = " << m_networkConfig.maxConnections << std::endl;
    oss << "enable_keep_alive = " << (m_networkConfig.enableKeepAlive ? "true" : "false") << std::endl;
    oss << "keep_alive_interval_ms = " << m_networkConfig.keepAliveIntervalMs << std::endl;
    oss << "enable_proxy = " << (m_networkConfig.enableProxy ? "true" : "false") << std::endl;
    oss << "proxy_host = \"" << m_networkConfig.proxyHost << "\"" << std::endl;
    oss << "proxy_port = " << m_networkConfig.proxyPort << std::endl;
    oss << "proxy_user = \"" << m_networkConfig.proxyUser << "\"" << std::endl;
    oss << "proxy_password = \"" << m_networkConfig.proxyPassword << "\"" << std::endl;

    return oss.str();
}

bool ClientConfigManager::parseFullTomlConfig(const std::string& tomlContent) {
    try {
        // 解析TOML字符串
        auto data = toml::parse_str(tomlContent);

        // 解析上传配置节 [upload]
        if (data.contains("upload")) {
            auto upload = data["upload"];

            // 基本配置
            if (upload.contains("server_host")) {
                m_uploadConfig.serverHost = upload["server_host"].as_string();
            }
            if (upload.contains("server_port")) {
                m_uploadConfig.serverPort = static_cast<int>(upload["server_port"].as_integer());
            }
            if (upload.contains("upload_protocol")) {
                m_uploadConfig.uploadProtocol = upload["upload_protocol"].as_string();
            }

            // 性能相关配置
            if (upload.contains("max_concurrent_uploads")) {
                m_uploadConfig.maxConcurrentUploads = static_cast<int>(upload["max_concurrent_uploads"].as_integer());
            }
            if (upload.contains("chunk_size")) {
                m_uploadConfig.chunkSize = static_cast<size_t>(upload["chunk_size"].as_integer());
            }
            if (upload.contains("timeout_seconds")) {
                m_uploadConfig.timeoutSeconds = static_cast<int>(upload["timeout_seconds"].as_integer());
            }

            // 重试配置
            if (upload.contains("retry_count")) {
                m_uploadConfig.retryCount = static_cast<int>(upload["retry_count"].as_integer());
            }
            if (upload.contains("retry_delay_ms")) {
                m_uploadConfig.retryDelayMs = static_cast<int>(upload["retry_delay_ms"].as_integer());
            }

            // 布尔选项
            if (upload.contains("enable_resume")) {
                m_uploadConfig.enableResume = upload["enable_resume"].as_boolean();
            }
            if (upload.contains("enable_compression")) {
                m_uploadConfig.enableCompression = upload["enable_compression"].as_boolean();
            }
            if (upload.contains("enable_checksum")) {
                m_uploadConfig.enableChecksum = upload["enable_checksum"].as_boolean();
            }

            // 枚举类型解析
            if (upload.contains("compression_algorithm")) {
                std::string algo = upload["compression_algorithm"].as_string();
                // 使用EnumConvert转换器，带默认值处理
                m_uploadConfig.compressionAlgo = ::StringToCompressionAlgorithmOrDefault(
                    algo, CompressionAlgorithm::ZSTD);
            }
            if (upload.contains("checksum_algorithm")) {
                std::string algo = upload["checksum_algorithm"].as_string();
                // 使用EnumConvert转换器，带默认值处理
                m_uploadConfig.checksumAlgo = ::StringToChecksumAlgorithmOrDefault(
                    algo, ChecksumAlgorithm::SHA256);
            }
        }

        g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "TOML配置解析成功");
        return true;

    }
    catch (const std::exception& e) {
        m_lastError = "TOML解析异常: " + std::string(e.what());
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, m_lastError);
        return false;
    }
}

