#ifndef CLIENT_CONFIG_MANAGER_H
#define CLIENT_CONFIG_MANAGER_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include <mutex>
#include "utils/EnumConvert.hpp"
#ifdef _MSC_VER
// utf-8
#pragma execution_character_set("utf-8")
#endif // _MSC_VER


#define USER_AGENT                  "HesUploader/1.0"
#define DEFAULT_CONFIG_PATH         "./config/upload_client.toml"
#define DEFAULT_LOG_PATH            "./logs/upload_client.log"
#define CLIENT_VERSION              "1.0.0"

// TOML库依赖 - 使用项目内置toml11库
#include <toml11/toml.hpp>

/**
 * @enum CompressionAlgorithm
 * @brief 文件传输中可选的压缩算法
 *
 * 各算法在压缩率、速度、兼容性上有所不同：
 * - NONE: 无压缩，延迟最低，带宽占用大
 * - GZIP: 兼容性最佳，速度和压缩率中等
 * - ZSTD: 压缩率和速度兼顾，现代高效算法，推荐默认
 * - LZ4: 压缩率低但极快，适合实时场景
 * - BROTLI: 压缩率高，适合小文件和Web资源
 * - LZMA: 压缩率最高，但速度慢，适合归档存储
 */
enum class CompressionAlgorithm {
    NONE,   ///< 不压缩，CPU零开销，带宽占用大，适合局域网或CPU紧张场景
    GZIP,   ///< GZIP(Deflate)算法，跨平台支持好，压缩率/速度均衡，Web传输常用
    ZSTD,   ///< Zstandard，压缩/解压速度快，压缩率优于GZIP，推荐默认
    LZ4,    ///< LZ4，极致速度，压缩率较低，适合实时/低延迟场景
    BROTLI, ///< Brotli，压缩率高于GZIP，Web优化，小文件表现好，压缩速度较慢
    LZMA,   ///< LZMA，压缩率最高但速度慢，适合归档/离线传输
};



/**
 * @enum ChecksumAlgorithm
 * @brief 文件传输完整性校验可选算法
 *
 * - NONE: 不进行校验，性能最高但无安全保证
 * - CRC32: 快速校验，检测随机错误，但抗碰撞性弱
 * - MD5: 速度快，常用于文件一致性校验，但已不安全
 * - SHA1: 安全性强于MD5，但已逐渐弃用
 * - SHA256: 安全性和通用性较好，推荐默认
 * - SHA512: 安全性更强，适合高安全要求场景
 * - BLAKE2: 现代高性能哈希，比SHA系列更快，安全性好
 */
enum class ChecksumAlgorithm {
    NONE,    ///< 不做校验，适合内部可信环境
    CRC32,   ///< 循环冗余校验，速度快，主要检测传输错误
    MD5,     ///< 128位哈希，常用于校验文件一致性（不安全）
    SHA1,    ///< 160位哈希，抗碰撞性弱，逐渐弃用
    SHA256,  ///< 256位哈希，兼顾速度与安全，推荐默认
    SHA512,  ///< 512位哈希，安全性更高，适合高安全要求
    BLAKE2,  ///< 新一代哈希，比SHA256快，安全性强
};

// 使用EnumConvert生成转换器支持
DEFINE_ENUM_SUPPORT(CompressionAlgorithm)
DEFINE_ENUM_SUPPORT(ChecksumAlgorithm)

// 枚举转换现在由EnumConvert转换器提供
// 可以使用: CompressionAlgorithmToString(), StringToCompressionAlgorithm() 等函数
// 或者直接使用流操作符: std::cout << algo; std::cin >> algo;

// TOML配置节名称常量
namespace ConfigSections {
    const std::string UPLOAD = "upload";
    const std::string UI = "ui";
    const std::string NETWORK = "network";
    const std::string LOGGING = "logging";
    const std::string SECURITY = "security";
}



/**
 * @brief 客户端配置管理器 - 统一管理所有配置项
 *
 * 负责配置文件的加载、保存、验证和运行时配置管理。
 * 支持TOML格式配置文件，提供配置变更回调机制。
 * TOML格式具有更好的可读性和层次结构，适合复杂配置管理。
 *
 * @author hesphoros
 * @date 2025-09-30
 * @version 1.0.0
 */
class ClientConfigManager {
public:

    /**
      * @brief 上传配置结构体 - 包含所有上传相关配置
      */
    struct UploadConfig {
        // ===================== 基础网络配置 =====================
        std::string serverHost          = "127.0.0.1";      // 服务器地址
        uint16_t    serverPort          = 9000;             // 服务器端口
        std::string uploadProtocol      = "TCP";            // 上传协议: HTTP/FTP/gRPC/WebSocket

        // ===================== 上传控制 =====================
        uint32_t maxConcurrentUploads   = 4;            // 最大并发上传任务数
        uint32_t chunkSize              = 1024 * 1024;  // 分块大小（默认1MB）
        uint32_t timeoutSeconds         = 30;           // 单个请求超时时间
        uint32_t retryCount             = 3;            // 上传失败重试次数
        uint32_t retryDelayMs           = 1000;         // 重试间隔（毫秒）
        uint64_t maxUploadSpeed         = 0;            // 最大上传速率 (bytes/s, 0=不限速)
        uint64_t maxFileSize            = 0;            // 限制单文件最大大小 (0=不限)

        // ===================== 功能开关 =====================
        bool enableResume               = true;         // 是否启用断点续传
        bool enableCompression          = true;         // 是否对分块数据压缩
        CompressionAlgorithm compressionAlgo = CompressionAlgorithm::GZIP; // 压缩算法
        bool enableChecksum             = true;         // 是否启用完整性校验
        ChecksumAlgorithm checksumAlgo  = ChecksumAlgorithm::MD5;  // 校验算法
        bool overwrite                  = false;        // 是否覆盖服务器已存在的文件
        bool enableMultipart            = true;         // 是否启用多部分表单上传
        bool enableProgress             = true;         // 是否启用进度回调

        // ===================== 文件相关 =====================
        std::string targetDir = "/uploads";   // 服务器目标目录


        bool useSSL                     = false;        // 是否启用 SSL/TLS
        std::string certFile            = "";           // 客户端证书文件
        std::string privateKeyFile      = "";           // 私钥文件
        std::string caFile              = "";           // CA 证书路径
        bool verifyServer               = true;         // 是否校验服务器证书
        std::string authToken           = "";           // 上传鉴权 Token

        std::string logLevel            = "";           // 日志级别: DEBUG/INFO/WARN/ERROR
        std::string logFilePath         = "";           // 日志文件路径 (为空=只输出到控制台)
        bool enableDetailedLog          = false;        // 是否输出详细日志 (每个分块/重试)

        // ===================== 扩展 =====================
        std::string clientVersion       = CLIENT_VERSION;      // 客户端版本号，用于服务端识别
        std::string userAgent           = USER_AGENT;          // 自定义 User-Agent (HTTP类协议)
        std::vector<std::string> excludePatterns; // 排除的文件模式 (*.tmp, *.bak)
    };

    /**
   * @brief UI配置结构体 - 用户界面相关配置
   */
    struct UIConfig {
        bool showProgressDetails = true;      // 显示详细进度信息
        bool showSpeedInfo       = true;      // 显示速度信息
        bool autoStartUpload     = true;      // 自动开始上传
        bool minimizeToTray      = false;     // 最小化到系统托盘
        bool showNotifications   = true;      // 显示系统通知

        std::string language     = "zh-CN";   // 界面语言
        std::string theme        = "default"; // 主题样式

        // 窗口状态
        int windowWidth          = 1000;
        int windowHeight         = 700;
        bool windowMaximized     = false;

        // 文件列表显示
        bool showFileSize        = true;
        bool showFileType        = true;
        bool showUploadTime      = true;
        bool showFileStatus      = true;
    };

    /**
     * @brief 网络配置结构体 - 网络相关配置
     */
    struct NetworkConfig {
        uint32_t connectTimeoutMs        = 5000;   // 连接超时时间
        uint32_t readTimeoutMs           = 30000;  // 读取超时时间
        uint32_t writeTimeoutMs          = 30000;  // 写入超时时间

        uint32_t bufferSize              = 8192;   // 网络缓冲区大小
        uint32_t maxConnections          = 10;     // 最大连接数
        bool     enableKeepAlive         = true;   // 启用Keep-Alive
        uint32_t keepAliveIntervalMs     = 30000;  // Keep-Alive间隔

        // 应用层心跳配置
        bool     enableAppHeartbeat      = true;   // 启用应用层心跳
        uint32_t heartbeatIntervalMs     = 10000;  // 心跳发送间隔（毫秒）
        uint32_t heartbeatTimeoutMs      = 30000;  // 心跳超时时间（毫秒）
        uint32_t heartbeatMaxFailures    = 3;      // 连续失败次数触发重连

        bool     enableAutoReconnect     = true;   // 启用自动重连
        uint32_t reconnectIntervalMs     = 1000;   // 重连间隔（毫秒）
        uint32_t maxReconnectAttempts    = 5;      // 最大重连尝试次数
        uint32_t reconnectBackoffMs      = 2000;   // 重连退避时间（毫秒）
        bool     enableReconnectBackoff  = true;   // 启用重连退避策略

        bool enableProxy                 = false;  // 启用代理
        std::string proxyHost            = "";     // 代理服务器地址
        uint16_t proxyPort               = 7890;   // 代理服务器端口
        std::string proxyUser            = "";     // 代理用户名
        std::string proxyPassword        = "";     // 代理密码
    };

    /**
     * @brief 配置变更回调函数类型
     */
    using ConfigChangeCallback = std::function<void(const std::string& section, const std::string& key)>;

public:
    /**
     * @brief 获取全局单例实例
     * @return ClientConfigManager实例引用
     */
    static ClientConfigManager& getInstance();

    /**
     * @brief 析构函数
     */
    ~ClientConfigManager();

    // ===================== 配置文件管理 =====================


    /**
     * @brief 重置为默认配置
     */
    void setDefaults();

    /**
     * @brief 验证配置有效性
     * @return 配置有效返回true，无效返回false
     */
    bool validateConfig() const;

    /**
     * @brief 获取配置验证错误信息
     * @return 错误信息列表
     */
    std::vector<std::string> getValidationErrors() const;

    // ===================== 配置访问器 =====================

    /**
     * @brief 获取上传配置
     * @return UploadConfig结构体引用
     */
    UploadConfig& getUploadConfig() { return m_uploadConfig; }

    /**
     * @brief 获取上传配置（只读）
     * @return UploadConfig结构体常量引用
     */
    const UploadConfig& getUploadConfig() const { return m_uploadConfig; }

    /**
     * @brief 获取UI配置
     * @return UIConfig结构体引用
     */
    UIConfig& getUIConfig() { return m_uiConfig; }

    /**
     * @brief 获取UI配置（只读）
     * @return UIConfig结构体常量引用
     */
    const UIConfig& getUIConfig() const { return m_uiConfig; }

    /**
     * @brief 获取网络配置
     * @return NetworkConfig结构体引用
     */
    NetworkConfig& getNetworkConfig() { return m_networkConfig; }

    /**
     * @brief 获取网络配置（只读）
     * @return NetworkConfig结构体常量引用
     */
    const NetworkConfig& getNetworkConfig() const { return m_networkConfig; }

    // ===================== 配置更新通知 =====================

    /**
     * @brief 注册配置变更回调函数
     * @param callback 回调函数
     */
    void setConfigChangeCallback(ConfigChangeCallback callback);

    /**
     * @brief 触发配置变更通知
     * @param section 配置节名
     * @param key 配置键名
     */
    void notifyConfigChanged(const std::string& section, const std::string& key);

    // ===================== 便捷方法 =====================

    /**
     * @brief 获取配置文件默认路径
     * @return 默认配置文件路径
     */
    std::string getDefaultConfigPath() const;

    /**
     * @brief 检查配置文件是否存在
     * @param configPath 配置文件路径
     * @return 文件存在返回true，不存在返回false
     */
    bool configFileExists(const std::string& configPath) const;

    /**
     * @brief 创建默认配置文件
     * @param configPath 配置文件路径
     * @return 创建成功返回true，失败返回false
     */
    bool createDefaultConfigFile(const std::string& configPath) const;

    /**
     * @brief 获取配置摘要信息（用于调试）
     * @return 配置摘要字符串
     */
    std::string getConfigSummary() const;

    // ===================== TOML特有功能 =====================

    /**
     * @brief 导出配置为TOML字符串（用于备份或传输）
     * @return TOML格式的配置字符串
     */
    std::string exportToTomlString() const;

    /**
     * @brief 从TOML字符串导入配置
     * @param tomlContent TOML配置内容
     * @return 导入成功返回true
     */
    bool importFromTomlString(const std::string& tomlContent);

    /**
     * @brief 从文件加载 TOML 配置
     * @param configPath 配置文件路径
     * @return 加载成功返回 true
     */
    bool loadFromFile(const std::string& configPath);

    /**
     * @brief 保存配置到文件
     * @param configPath 配置文件路径
     * @return 保存成功返回 true
     */
    bool saveToFile(const std::string& configPath) const;

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    ClientConfigManager();

    /**
     * @brief 禁用拷贝构造函数
     */
    ClientConfigManager(const ClientConfigManager&) = delete;

    /**
     * @brief 禁用赋值运算符
     */
    ClientConfigManager& operator=(const ClientConfigManager&) = delete;

    // ===================== 内部方法 =====================

    /**
     * @brief 初始化默认配置
     */
    void initializeDefaults();

    /**
     * @brief 验证上传配置
     * @param errors 错误信息收集器
     * @return 验证结果
     */
    bool validateUploadConfig(std::vector<std::string>& errors) const;

    /**
     * @brief 验证UI配置
     * @param errors 错误信息收集器
     * @return 验证结果
     */
    bool validateUIConfig(std::vector<std::string>& errors) const;

    /**
     * @brief 验证网络配置
     * @param errors 错误信息收集器
     * @return 验证结果
     */
    bool validateNetworkConfig(std::vector<std::string>& errors) const;

    // 内部简化的TOML序列化方法 - 不需要单独的序列化方法

    /**
     * @brief 生成完整的TOML配置文件内容
     * @return 完整TOML配置字符串
     */
    std::string generateFullTomlConfig() const;

    /**
     * @brief 从TOML文件解析所有配置节
     * @param tomlContent TOML文件内容
     * @return 解析成功返回true
     */
    bool parseFullTomlConfig(const std::string& tomlContent);

    /**
     * @brief 解析上传配置节
     */
    void parseUploadConfigSection(const toml::value& data);

    /**
     * @brief 解析UI配置节
     */
    void parseUIConfigSection(const toml::value& data);

    /**
     * @brief 解析网络配置节
     */
    void parseNetworkConfigSection(const toml::value& data);

    /**
     * @brief 通用配置解析工具函数 - 基础类型
     */
    template<typename T>
    void parseConfigValue(const toml::value& section, const std::string& key, T& target);

    /**
     * @brief 通用配置解析工具函数 - 枚举类型
     */
    template<typename EnumType>
    void parseEnumValue(const toml::value& section, const std::string& key, EnumType& target);

    /**
     * @brief 通用配置解析工具函数 - 字符串数组
     */
    void parseStringArray(const toml::value& section, const std::string& key, std::vector<std::string>& target);

private:
    // ===================== 成员变量 =====================
    UploadConfig                                    m_uploadConfig;      // 上传配置
    UIConfig                                        m_uiConfig;          // UI配置
    NetworkConfig                                   m_networkConfig;     // 网络配置
    ConfigChangeCallback                            m_changeCallback;    // 配置变更回调
    mutable std::string                             m_lastError;         // 最后的错误信息
    std::string                                     m_currentConfigPath; // 当前配置文件路径
    mutable std::mutex                              m_configMutex;       // 配置访问互斥锁
    static std::unique_ptr<ClientConfigManager>     m_instance;
    static std::mutex                               m_instanceMutex;

    /**
     * @brief 从 TOML 表中安全获取值
     */
    template<typename T>
    T getTomlValue(const toml::value& table, const std::string& key, const T& defaultValue) const;
};

#endif // CLIENT_CONFIG_MANAGER_H