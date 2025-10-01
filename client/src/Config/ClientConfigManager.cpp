#include "Config/ClientConfigManager.h"
#include "log_headers.h"
#include <mutex>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>
#include "utils/EnumConvert.hpp"

using namespace utils;
#include <toml11/toml.hpp>


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
    return "./config/upload_client.toml";
#else
    return "~/.config/upload_client/config.toml";
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

    // 网络配置摘要
    oss << "连接超时: " << m_networkConfig.connectTimeoutMs << " ms\n";
    oss << "缓冲区大小: " << (m_networkConfig.bufferSize / 1024) << " KB\n";
    oss << "自动重连: " << (m_networkConfig.enableAutoReconnect ? "启用" : "禁用") << "\n";
    oss << "重连间隔: " << m_networkConfig.reconnectIntervalMs << " ms\n";
    oss << "最大重连次数: " << m_networkConfig.maxReconnectAttempts << "\n";
    oss << "代理: " << (m_networkConfig.enableProxy ? "启用" : "禁用") << "\n";

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

    if (m_networkConfig.writeTimeoutMs < 5000 || m_networkConfig.writeTimeoutMs > 300000) {
        errors.push_back("写入超时时间应在5-300秒范围内");
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

    // 验证Keep-Alive间隔
    if (m_networkConfig.keepAliveIntervalMs < 1000 || m_networkConfig.keepAliveIntervalMs > 300000) {
        errors.push_back("Keep-Alive间隔应在1-300秒范围内");
        isValid = false;
    }

    // 验证重连配置
    if (m_networkConfig.reconnectIntervalMs < 100 || m_networkConfig.reconnectIntervalMs > 60000) {
        errors.push_back("重连间隔应在0.1-60秒范围内");
        isValid = false;
    }

    if (m_networkConfig.maxReconnectAttempts == 0 || m_networkConfig.maxReconnectAttempts > 100) {
        errors.push_back("最大重连尝试次数应在1-100范围内");
        isValid = false;
    }

    if (m_networkConfig.reconnectBackoffMs < 100 || m_networkConfig.reconnectBackoffMs > 300000) {
        errors.push_back("重连退避时间应在0.1-300秒范围内");
        isValid = false;
    }

    // 验证代理配置
    if (m_networkConfig.enableProxy) {
        if (m_networkConfig.proxyHost.empty()) {
            errors.push_back("启用代理时代理主机不能为空");
            isValid = false;
        }
        if (m_networkConfig.proxyPort == 0 || m_networkConfig.proxyPort > 65535) {
            errors.push_back("代理端口应在1-65535范围内");
            isValid = false;
        }
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

    // 重连配置
    oss << "enable_auto_reconnect = " << (m_networkConfig.enableAutoReconnect ? "true" : "false") << std::endl;
    oss << "reconnect_interval_ms = " << m_networkConfig.reconnectIntervalMs << std::endl;
    oss << "max_reconnect_attempts = " << m_networkConfig.maxReconnectAttempts << std::endl;
    oss << "reconnect_backoff_ms = " << m_networkConfig.reconnectBackoffMs << std::endl;
    oss << "enable_reconnect_backoff = " << (m_networkConfig.enableReconnectBackoff ? "true" : "false") << std::endl;

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

        // ===================== 解析上传配置节 [upload] =====================
        if (data.contains("upload")) {
            auto upload = data["upload"];

            // 基本网络配置
            if (upload.contains("server_host")) {
                m_uploadConfig.serverHost = upload["server_host"].as_string();
            }
            if (upload.contains("server_port")) {
                m_uploadConfig.serverPort = static_cast<uint16_t>(upload["server_port"].as_integer());
            }
            if (upload.contains("upload_protocol")) {
                m_uploadConfig.uploadProtocol = upload["upload_protocol"].as_string();
            }

            // 性能控制参数
            if (upload.contains("max_concurrent_uploads")) {
                m_uploadConfig.maxConcurrentUploads = static_cast<uint32_t>(upload["max_concurrent_uploads"].as_integer());
            }
            if (upload.contains("chunk_size")) {
                m_uploadConfig.chunkSize = static_cast<uint32_t>(upload["chunk_size"].as_integer());
            }
            if (upload.contains("timeout_seconds")) {
                m_uploadConfig.timeoutSeconds = static_cast<uint32_t>(upload["timeout_seconds"].as_integer());
            }

            // 重试策略
            if (upload.contains("retry_count")) {
                m_uploadConfig.retryCount = static_cast<uint32_t>(upload["retry_count"].as_integer());
            }
            if (upload.contains("retry_delay_ms")) {
                m_uploadConfig.retryDelayMs = static_cast<uint32_t>(upload["retry_delay_ms"].as_integer());
            }

            // 速度和大小限制
            if (upload.contains("max_upload_speed")) {
                m_uploadConfig.maxUploadSpeed = static_cast<uint64_t>(upload["max_upload_speed"].as_integer());
            }
            if (upload.contains("max_file_size")) {
                m_uploadConfig.maxFileSize = static_cast<uint64_t>(upload["max_file_size"].as_integer());
            }

            // 功能开关
            if (upload.contains("enable_resume")) {
                m_uploadConfig.enableResume = upload["enable_resume"].as_boolean();
            }
            if (upload.contains("enable_compression")) {
                m_uploadConfig.enableCompression = upload["enable_compression"].as_boolean();
            }
            if (upload.contains("enable_checksum")) {
                m_uploadConfig.enableChecksum = upload["enable_checksum"].as_boolean();
            }
            if (upload.contains("overwrite")) {
                m_uploadConfig.overwrite = upload["overwrite"].as_boolean();
            }
            if (upload.contains("enable_multipart")) {
                m_uploadConfig.enableMultipart = upload["enable_multipart"].as_boolean();
            }
            if (upload.contains("enable_progress")) {
                m_uploadConfig.enableProgress = upload["enable_progress"].as_boolean();
            }

            // 枚举类型解析
            if (upload.contains("compression_algorithm")) {
                std::string algo = upload["compression_algorithm"].as_string();
                auto converted = StringToCompressionAlgorithm(algo);
                if (converted.has_value()) {
                    m_uploadConfig.compressionAlgo = converted.value();
                }
            }
            if (upload.contains("checksum_algorithm")) {
                std::string algo = upload["checksum_algorithm"].as_string();
                auto converted = StringToChecksumAlgorithm(algo);
                if (converted.has_value()) {
                    m_uploadConfig.checksumAlgo = converted.value();
                }
            }

            // 文件和路径配置
            if (upload.contains("target_dir")) {
                m_uploadConfig.targetDir = upload["target_dir"].as_string();
            }

            // 排除模式数组
            if (upload.contains("exclude_patterns") && upload["exclude_patterns"].is_array()) {
                m_uploadConfig.excludePatterns.clear();
                auto patterns = upload["exclude_patterns"].as_array();
                for (const auto& pattern : patterns) {
                    m_uploadConfig.excludePatterns.push_back(pattern.as_string());
                }
            }

            // SSL/TLS安全配置
            if (upload.contains("use_ssl")) {
                m_uploadConfig.useSSL = upload["use_ssl"].as_boolean();
            }
            if (upload.contains("cert_file")) {
                m_uploadConfig.certFile = upload["cert_file"].as_string();
            }
            if (upload.contains("private_key_file")) {
                m_uploadConfig.privateKeyFile = upload["private_key_file"].as_string();
            }
            if (upload.contains("ca_file")) {
                m_uploadConfig.caFile = upload["ca_file"].as_string();
            }
            if (upload.contains("verify_server")) {
                m_uploadConfig.verifyServer = upload["verify_server"].as_boolean();
            }
            if (upload.contains("auth_token")) {
                m_uploadConfig.authToken = upload["auth_token"].as_string();
            }

            // 日志配置
            if (upload.contains("log_level")) {
                m_uploadConfig.logLevel = upload["log_level"].as_string();
            }
            if (upload.contains("log_file_path")) {
                m_uploadConfig.logFilePath = upload["log_file_path"].as_string();
            }
            if (upload.contains("enable_detailed_log")) {
                m_uploadConfig.enableDetailedLog = upload["enable_detailed_log"].as_boolean();
            }

            // 扩展信息
            if (upload.contains("client_version")) {
                m_uploadConfig.clientVersion = upload["client_version"].as_string();
            }
            if (upload.contains("user_agent")) {
                m_uploadConfig.userAgent = upload["user_agent"].as_string();
            }
        }

        // ===================== 解析UI配置节 [ui] =====================
        if (data.contains("ui")) {
            auto ui = data["ui"];

            // 进度和显示配置
            if (ui.contains("show_progress_details")) {
                m_uiConfig.showProgressDetails = ui["show_progress_details"].as_boolean();
            }
            if (ui.contains("show_speed_info")) {
                m_uiConfig.showSpeedInfo = ui["show_speed_info"].as_boolean();
            }
            if (ui.contains("auto_start_upload")) {
                m_uiConfig.autoStartUpload = ui["auto_start_upload"].as_boolean();
            }
            if (ui.contains("minimize_to_tray")) {
                m_uiConfig.minimizeToTray = ui["minimize_to_tray"].as_boolean();
            }
            if (ui.contains("show_notifications")) {
                m_uiConfig.showNotifications = ui["show_notifications"].as_boolean();
            }

            // 界面设置
            if (ui.contains("language")) {
                m_uiConfig.language = ui["language"].as_string();
            }
            if (ui.contains("theme")) {
                m_uiConfig.theme = ui["theme"].as_string();
            }

            // 窗口状态
            if (ui.contains("window_width")) {
                m_uiConfig.windowWidth = static_cast<int>(ui["window_width"].as_integer());
            }
            if (ui.contains("window_height")) {
                m_uiConfig.windowHeight = static_cast<int>(ui["window_height"].as_integer());
            }
            if (ui.contains("window_maximized")) {
                m_uiConfig.windowMaximized = ui["window_maximized"].as_boolean();
            }

            // 文件列表显示
            if (ui.contains("show_file_size")) {
                m_uiConfig.showFileSize = ui["show_file_size"].as_boolean();
            }
            if (ui.contains("show_file_type")) {
                m_uiConfig.showFileType = ui["show_file_type"].as_boolean();
            }
            if (ui.contains("show_upload_time")) {
                m_uiConfig.showUploadTime = ui["show_upload_time"].as_boolean();
            }
            if (ui.contains("show_file_status")) {
                m_uiConfig.showFileStatus = ui["show_file_status"].as_boolean();
            }
        }

        // ===================== 解析网络配置节 [network] =====================
        if (data.contains("network")) {
            auto network = data["network"];

            // 超时设置
            if (network.contains("connect_timeout_ms")) {
                m_networkConfig.connectTimeoutMs = static_cast<uint32_t>(network["connect_timeout_ms"].as_integer());
            }
            if (network.contains("read_timeout_ms")) {
                m_networkConfig.readTimeoutMs = static_cast<uint32_t>(network["read_timeout_ms"].as_integer());
            }
            if (network.contains("write_timeout_ms")) {
                m_networkConfig.writeTimeoutMs = static_cast<uint32_t>(network["write_timeout_ms"].as_integer());
            }

            // 连接管理
            if (network.contains("buffer_size")) {
                m_networkConfig.bufferSize = static_cast<uint32_t>(network["buffer_size"].as_integer());
            }
            if (network.contains("max_connections")) {
                m_networkConfig.maxConnections = static_cast<uint32_t>(network["max_connections"].as_integer());
            }

            // Keep-Alive配置
            if (network.contains("enable_keep_alive")) {
                m_networkConfig.enableKeepAlive = network["enable_keep_alive"].as_boolean();
            }
            if (network.contains("keep_alive_interval_ms")) {
                m_networkConfig.keepAliveIntervalMs = static_cast<uint32_t>(network["keep_alive_interval_ms"].as_integer());
            }

            // 重连配置
            if (network.contains("enable_auto_reconnect")) {
                m_networkConfig.enableAutoReconnect = network["enable_auto_reconnect"].as_boolean();
            }
            if (network.contains("reconnect_interval_ms")) {
                m_networkConfig.reconnectIntervalMs = static_cast<uint32_t>(network["reconnect_interval_ms"].as_integer());
            }
            if (network.contains("max_reconnect_attempts")) {
                m_networkConfig.maxReconnectAttempts = static_cast<uint32_t>(network["max_reconnect_attempts"].as_integer());
            }
            if (network.contains("reconnect_backoff_ms")) {
                m_networkConfig.reconnectBackoffMs = static_cast<uint32_t>(network["reconnect_backoff_ms"].as_integer());
            }
            if (network.contains("enable_reconnect_backoff")) {
                m_networkConfig.enableReconnectBackoff = network["enable_reconnect_backoff"].as_boolean();
            }

            // 代理设置
            if (network.contains("enable_proxy")) {
                m_networkConfig.enableProxy = network["enable_proxy"].as_boolean();
            }
            if (network.contains("proxy_host")) {
                m_networkConfig.proxyHost = network["proxy_host"].as_string();
            }
            if (network.contains("proxy_port")) {
                m_networkConfig.proxyPort = static_cast<uint16_t>(network["proxy_port"].as_integer());
            }
            if (network.contains("proxy_user")) {
                m_networkConfig.proxyUser = network["proxy_user"].as_string();
            }
            if (network.contains("proxy_password")) {
                m_networkConfig.proxyPassword = network["proxy_password"].as_string();
            }
        }

        g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "TOML配置解析成功 - 已解析所有配置节");
        return true;

    }
    catch (const std::exception& e) {
        m_lastError = "TOML解析异常: " + std::string(e.what());
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, m_lastError);
        return false;
    }
}

