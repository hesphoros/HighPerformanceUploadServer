/**
 * @file spdlog_example.cpp
 * @brief 展示如何在ClientConfigManager中集成spdlog
 * @date 2025-10-01
 */

#include "Config/ClientConfigManager.h"
#include "spdlog_integration.h"
#include <spdlog/spdlog.h>

// 这是一个示例文件，展示如何替换现有的日志系统

namespace {

/**
 * @brief 配置管理器专用日志器
 */
std::shared_ptr<spdlog::logger> g_configLogger;

/**
 * @brief 初始化配置管理器日志
 */
void InitConfigLogger() {
    if (!g_configLogger) {
        g_configLogger = UploadClient::SpdlogManager::CreateLogger(
            "config", "logs/config.log");
        if (!g_configLogger) {
            g_configLogger = spdlog::default_logger();
        }
    }
}

} // anonymous namespace

/**
 * @brief 在ClientConfigManager中使用spdlog的示例方法
 */
class ClientConfigManagerSpdlogExample {
public:
    bool loadConfigurationWithSpdlog(const std::string& configPath) {
        // 确保日志器已初始化
        InitConfigLogger();
        
        // 性能计时
        PERF_TIMER("配置文件加载");
        
        g_configLogger->info("开始加载配置文件: {}", configPath);
        
        try {
            // 检查文件是否存在
            if (!std::filesystem::exists(configPath)) {
                g_configLogger->error("配置文件不存在: {}", configPath);
                return false;
            }
            
            // 获取文件大小
            auto fileSize = std::filesystem::file_size(configPath);
            g_configLogger->debug("配置文件大小: {} bytes", fileSize);
            
            // 读取文件内容
            std::ifstream file(configPath);
            if (!file.is_open()) {
                g_configLogger->error("无法打开配置文件: {}", configPath);
                return false;
            }
            
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            file.close();
            
            g_configLogger->debug("成功读取配置文件，内容长度: {} 字符", content.length());
            
            // 解析TOML配置
#ifdef TOML11_AVAILABLE
            auto data = toml::parse_str(content);
            
            // 日志解析过程
            if (data.contains("upload")) {
                g_configLogger->info("找到 [upload] 配置节");
                auto upload = data["upload"];
                
                if (upload.contains("server_host")) {
                    std::string host = upload["server_host"].as_string();
                    g_configLogger->info("服务器地址: {}", host);
                }
                
                if (upload.contains("server_port")) {
                    int port = static_cast<int>(upload["server_port"].as_integer());
                    g_configLogger->info("服务器端口: {}", port);
                }
                
                if (upload.contains("max_concurrent_uploads")) {
                    int maxUploads = static_cast<int>(upload["max_concurrent_uploads"].as_integer());
                    g_configLogger->info("最大并发上传数: {}", maxUploads);
                    
                    if (maxUploads > 16) {
                        g_configLogger->warn("并发上传数过高 ({}), 建议不超过16", maxUploads);
                    }
                }
                
                // 验证压缩算法
                if (upload.contains("compression_algorithm")) {
                    std::string algo = upload["compression_algorithm"].as_string();
                    g_configLogger->debug("压缩算法: {}", algo);
                    
                    std::vector<std::string> validAlgos = {"none", "gzip", "deflate", "lz4", "brotli"};
                    bool isValid = std::find(validAlgos.begin(), validAlgos.end(), algo) != validAlgos.end();
                    
                    if (!isValid) {
                        g_configLogger->error("无效的压缩算法: {}", algo);
                        g_configLogger->info("有效的压缩算法: {}", fmt::join(validAlgos, ", "));
                        return false;
                    }
                }
            } else {
                g_configLogger->warn("配置文件缺少 [upload] 节");
            }
            
            if (data.contains("ui")) {
                g_configLogger->info("找到 [ui] 配置节");
                auto ui = data["ui"];
                
                if (ui.contains("language")) {
                    std::string lang = ui["language"].as_string();
                    g_configLogger->info("界面语言: {}", lang);
                }
            }
            
            if (data.contains("network")) {
                g_configLogger->info("找到 [network] 配置节");
                auto network = data["network"];
                
                if (network.contains("connect_timeout_ms")) {
                    int timeout = static_cast<int>(network["connect_timeout_ms"].as_integer());
                    g_configLogger->info("连接超时: {}ms", timeout);
                    
                    if (timeout < 1000) {
                        g_configLogger->warn("连接超时过短 ({}ms), 建议至少1000ms", timeout);
                    }
                }
            }
            
            g_configLogger->info("✅ TOML配置解析成功");
            return true;
            
#else
            g_configLogger->warn("TOML支持未编译，跳过配置解析");
            return false;
#endif
            
        } catch (const std::exception& e) {
            g_configLogger->critical("配置加载异常: {}", e.what());
            return false;
        }
    }
    
    /**
     * @brief 保存配置示例
     */
    bool saveConfigurationWithSpdlog(const std::string& configPath) {
        InitConfigLogger();
        
        g_configLogger->info("开始保存配置到: {}", configPath);
        
        try {
            // 创建目录
            auto dir = std::filesystem::path(configPath).parent_path();
            if (!dir.empty() && !std::filesystem::exists(dir)) {
                std::filesystem::create_directories(dir);
                g_configLogger->debug("创建目录: {}", dir.string());
            }
            
            // 生成TOML内容
            std::string tomlContent = generateTomlConfig();
            
            // 写入文件
            std::ofstream file(configPath);
            if (!file.is_open()) {
                g_configLogger->error("无法创建配置文件: {}", configPath);
                return false;
            }
            
            file << tomlContent;
            file.close();
            
            g_configLogger->info("✅ 配置保存成功，文件大小: {} bytes", tomlContent.length());
            return true;
            
        } catch (const std::exception& e) {
            g_configLogger->critical("配置保存异常: {}", e.what());
            return false;
        }
    }
    
private:
    std::string generateTomlConfig() {
        // 这里是生成TOML配置的示例代码
        return R"(# 高性能上传客户端配置
[upload]
server_host = "localhost"
server_port = 8080
max_concurrent_uploads = 4
compression_algorithm = "gzip"

[ui]
language = "zh_CN"
theme = "default"

[network]
connect_timeout_ms = 5000
)";
    }
};

/**
 * @brief 使用示例 - 如何在main函数中初始化
 */
void ExampleUsage() {
    // 1. 初始化spdlog系统
    if (!UploadClient::SpdlogManager::Initialize("logs", "info")) {
        std::cerr << "日志系统初始化失败" << std::endl;
        return;
    }
    
    // 2. 使用配置管理器
    ClientConfigManagerSpdlogExample configMgr;
    
    // 3. 加载配置
    if (configMgr.loadConfigurationWithSpdlog("config/upload_client.toml")) {
        LOG_INFO("应用程序配置加载成功");
    } else {
        LOG_ERROR("应用程序配置加载失败");
    }
    
    // 4. 保存配置
    if (configMgr.saveConfigurationWithSpdlog("config/output.toml")) {
        LOG_INFO("配置保存成功");
    }
    
    // 5. 关闭日志系统
    UploadClient::SpdlogManager::Shutdown();
}