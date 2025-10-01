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

        // 使用映射表和抽象函数来减少重复代码
        parseUploadConfigSection(data);
        parseUIConfigSection(data);
        parseNetworkConfigSection(data);

        g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "TOML配置解析成功 - 已解析所有配置节");
        return true;

    }
    catch (const std::exception& e) {
        m_lastError = "TOML解析异常: " + std::string(e.what());
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, m_lastError);
        return false;
    }
}

// ===================== 优化的TOML解析方法实现 =====================

template<typename T>
void ClientConfigManager::parseConfigValue(const toml::value& section, const std::string& key, T& target) {
    if (section.contains(key)) {
        try {
            if constexpr (std::is_same_v<T, std::string>) {
                target = section.at(key).as_string();
            }
            else if constexpr (std::is_same_v<T, bool>) {
                target = section.at(key).as_boolean();
            }
            else if constexpr (std::is_same_v<T, int>) {
                target = static_cast<int>(section.at(key).as_integer());
            }
            else if constexpr (std::is_same_v<T, uint16_t>) {
                target = static_cast<uint16_t>(section.at(key).as_integer());
            }
            else if constexpr (std::is_same_v<T, uint32_t>) {
                target = static_cast<uint32_t>(section.at(key).as_integer());
            }
            else if constexpr (std::is_same_v<T, uint64_t>) {
                target = static_cast<uint64_t>(section.at(key).as_integer());
            }
        }
        catch (const std::exception& e) {
            g_luspLogWriteImpl.WriteLogContent(LOG_WARN,
                "解析配置项失败 [" + key + "]: " + e.what());
        }
    }
}

template<typename EnumType>
void ClientConfigManager::parseEnumValue(const toml::value& section, const std::string& key, EnumType& target) {
    if (section.contains(key)) {
        try {
            std::string enumStr = section.at(key).as_string();
            if constexpr (std::is_same_v<EnumType, CompressionAlgorithm>) {
                auto converted = StringToCompressionAlgorithm(enumStr);
                if (converted.has_value()) {
                    target = converted.value();
                }
            }
            else if constexpr (std::is_same_v<EnumType, ChecksumAlgorithm>) {
                auto converted = StringToChecksumAlgorithm(enumStr);
                if (converted.has_value()) {
                    target = converted.value();
                }
            }
        }
        catch (const std::exception& e) {
            g_luspLogWriteImpl.WriteLogContent(LOG_WARN,
                "解析枚举配置项失败 [" + key + "]: " + e.what());
        }
    }
}

void ClientConfigManager::parseStringArray(const toml::value& section, const std::string& key, std::vector<std::string>& target) {
    if (section.contains(key) && section.at(key).is_array()) {
        try {
            target.clear();
            auto array = section.at(key).as_array();
            for (const auto& item : array) {
                target.push_back(item.as_string());
            }
        }
        catch (const std::exception& e) {
            g_luspLogWriteImpl.WriteLogContent(LOG_WARN,
                "解析字符串数组配置项失败 [" + key + "]: " + e.what());
        }
    }
}

void ClientConfigManager::parseUploadConfigSection(const toml::value& data) {
    if (!data.contains("upload")) return;

    auto upload = data.at("upload");

    // 使用抽象工具函数来减少重复代码 - 基本配置
    parseConfigValue(upload, "server_host", m_uploadConfig.serverHost);
    parseConfigValue(upload, "server_port", m_uploadConfig.serverPort);
    parseConfigValue(upload, "upload_protocol", m_uploadConfig.uploadProtocol);

    // 性能控制参数
    parseConfigValue(upload, "max_concurrent_uploads", m_uploadConfig.maxConcurrentUploads);
    parseConfigValue(upload, "chunk_size", m_uploadConfig.chunkSize);
    parseConfigValue(upload, "timeout_seconds", m_uploadConfig.timeoutSeconds);

    // 重试策略
    parseConfigValue(upload, "retry_count", m_uploadConfig.retryCount);
    parseConfigValue(upload, "retry_delay_ms", m_uploadConfig.retryDelayMs);

    // 速度和大小限制
    parseConfigValue(upload, "max_upload_speed", m_uploadConfig.maxUploadSpeed);
    parseConfigValue(upload, "max_file_size", m_uploadConfig.maxFileSize);

    // 功能开关
    parseConfigValue(upload, "enable_resume", m_uploadConfig.enableResume);
    parseConfigValue(upload, "enable_compression", m_uploadConfig.enableCompression);
    parseConfigValue(upload, "enable_checksum", m_uploadConfig.enableChecksum);
    parseConfigValue(upload, "overwrite", m_uploadConfig.overwrite);
    parseConfigValue(upload, "enable_multipart", m_uploadConfig.enableMultipart);
    parseConfigValue(upload, "enable_progress", m_uploadConfig.enableProgress);

    // 枚举类型
    parseEnumValue(upload, "compression_algorithm", m_uploadConfig.compressionAlgo);
    parseEnumValue(upload, "checksum_algorithm", m_uploadConfig.checksumAlgo);

    // 文件和路径配置
    parseConfigValue(upload, "target_dir", m_uploadConfig.targetDir);

    // SSL/TLS安全配置
    parseConfigValue(upload, "use_ssl", m_uploadConfig.useSSL);
    parseConfigValue(upload, "cert_file", m_uploadConfig.certFile);
    parseConfigValue(upload, "private_key_file", m_uploadConfig.privateKeyFile);
    parseConfigValue(upload, "ca_file", m_uploadConfig.caFile);
    parseConfigValue(upload, "verify_server", m_uploadConfig.verifyServer);
    parseConfigValue(upload, "auth_token", m_uploadConfig.authToken);

    // 日志配置
    parseConfigValue(upload, "log_level", m_uploadConfig.logLevel);
    parseConfigValue(upload, "log_file_path", m_uploadConfig.logFilePath);
    parseConfigValue(upload, "enable_detailed_log", m_uploadConfig.enableDetailedLog);

    // 扩展信息
    parseConfigValue(upload, "client_version", m_uploadConfig.clientVersion);
    parseConfigValue(upload, "user_agent", m_uploadConfig.userAgent);

    // 特殊处理：字符串数组
    parseStringArray(upload, "exclude_patterns", m_uploadConfig.excludePatterns);
}

void ClientConfigManager::parseUIConfigSection(const toml::value& data) {
    if (!data.contains("ui")) return;

    auto ui = data.at("ui");

    // 进度和显示配置
    parseConfigValue(ui, "show_progress_details", m_uiConfig.showProgressDetails);
    parseConfigValue(ui, "show_speed_info", m_uiConfig.showSpeedInfo);
    parseConfigValue(ui, "auto_start_upload", m_uiConfig.autoStartUpload);
    parseConfigValue(ui, "minimize_to_tray", m_uiConfig.minimizeToTray);
    parseConfigValue(ui, "show_notifications", m_uiConfig.showNotifications);

    // 界面设置
    parseConfigValue(ui, "language", m_uiConfig.language);
    parseConfigValue(ui, "theme", m_uiConfig.theme);

    // 窗口状态
    parseConfigValue(ui, "window_width", m_uiConfig.windowWidth);
    parseConfigValue(ui, "window_height", m_uiConfig.windowHeight);
    parseConfigValue(ui, "window_maximized", m_uiConfig.windowMaximized);

    // 文件列表显示
    parseConfigValue(ui, "show_file_size", m_uiConfig.showFileSize);
    parseConfigValue(ui, "show_file_type", m_uiConfig.showFileType);
    parseConfigValue(ui, "show_upload_time", m_uiConfig.showUploadTime);
    parseConfigValue(ui, "show_file_status", m_uiConfig.showFileStatus);
}

void ClientConfigManager::parseNetworkConfigSection(const toml::value& data) {
    if (!data.contains("network")) return;

    auto network = data.at("network");

    // 超时设置
    parseConfigValue(network, "connect_timeout_ms", m_networkConfig.connectTimeoutMs);
    parseConfigValue(network, "read_timeout_ms", m_networkConfig.readTimeoutMs);
    parseConfigValue(network, "write_timeout_ms", m_networkConfig.writeTimeoutMs);

    // 连接管理
    parseConfigValue(network, "buffer_size", m_networkConfig.bufferSize);
    parseConfigValue(network, "max_connections", m_networkConfig.maxConnections);

    // Keep-Alive配置
    parseConfigValue(network, "enable_keep_alive", m_networkConfig.enableKeepAlive);
    parseConfigValue(network, "keep_alive_interval_ms", m_networkConfig.keepAliveIntervalMs);

    // 重连配置
    parseConfigValue(network, "enable_auto_reconnect", m_networkConfig.enableAutoReconnect);
    parseConfigValue(network, "reconnect_interval_ms", m_networkConfig.reconnectIntervalMs);
    parseConfigValue(network, "max_reconnect_attempts", m_networkConfig.maxReconnectAttempts);
    parseConfigValue(network, "reconnect_backoff_ms", m_networkConfig.reconnectBackoffMs);
    parseConfigValue(network, "enable_reconnect_backoff", m_networkConfig.enableReconnectBackoff);

    // 代理设置
    parseConfigValue(network, "enable_proxy", m_networkConfig.enableProxy);
    parseConfigValue(network, "proxy_host", m_networkConfig.proxyHost);
    parseConfigValue(network, "proxy_port", m_networkConfig.proxyPort);
    parseConfigValue(network, "proxy_user", m_networkConfig.proxyUser);
    parseConfigValue(network, "proxy_password", m_networkConfig.proxyPassword);
}

// 模板实例化
template void ClientConfigManager::parseConfigValue<std::string>(const toml::value&, const std::string&, std::string&);
template void ClientConfigManager::parseConfigValue<bool>(const toml::value&, const std::string&, bool&);
template void ClientConfigManager::parseConfigValue<int>(const toml::value&, const std::string&, int&);
template void ClientConfigManager::parseConfigValue<uint16_t>(const toml::value&, const std::string&, uint16_t&);
template void ClientConfigManager::parseConfigValue<uint32_t>(const toml::value&, const std::string&, uint32_t&);
template void ClientConfigManager::parseConfigValue<uint64_t>(const toml::value&, const std::string&, uint64_t&);

template void ClientConfigManager::parseEnumValue<CompressionAlgorithm>(const toml::value&, const std::string&, CompressionAlgorithm&);
template void ClientConfigManager::parseEnumValue<ChecksumAlgorithm>(const toml::value&, const std::string&, ChecksumAlgorithm&);

