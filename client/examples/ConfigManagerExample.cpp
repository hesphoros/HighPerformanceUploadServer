/**
 * @file ConfigManagerExample.cpp
 * @brief ClientConfigManager使用示例
 * 
 * 展示如何使用配置管理器进行配置的加载、保存、验证等操作
 * 
 * @author hesphoros
 * @date 2025-09-30
 */

#include "Config/ClientConfigManager.h"
#include <iostream>

void demonstrateBasicUsage() {
    std::cout << "=== ClientConfigManager 基础使用示例 ===" << std::endl;
    
    // 获取配置管理器单例
    auto& config = ClientConfigManager::getInstance();
    
    // 显示默认配置摘要
    std::cout << config.getConfigSummary() << std::endl;
    
    // 修改上传配置
    auto& uploadConfig = config.getUploadConfig();
    uploadConfig.serverHost = "upload.example.com";
    uploadConfig.serverPort = 8080;
    uploadConfig.maxConcurrentUploads = 8;
    uploadConfig.enableResume = true;
    uploadConfig.useSSL = true;
    
    // 修改UI配置
    auto& uiConfig = config.getUIConfig();
    uiConfig.language = "en-US";
    uiConfig.theme = "dark";
    uiConfig.showProgressDetails = true;
    
    std::cout << "\n=== 修改后的配置 ===" << std::endl;
    std::cout << config.getConfigSummary() << std::endl;
}

void demonstrateConfigValidation() {
    std::cout << "\n=== 配置验证示例 ===" << std::endl;
    
    auto& config = ClientConfigManager::getInstance();
    
    // 设置一些无效配置来演示验证功能
    auto& uploadConfig = config.getUploadConfig();
    uploadConfig.serverPort = 70000;  // 无效端口
    uploadConfig.maxConcurrentUploads = 100;  // 超过限制
    uploadConfig.uploadProtocol = "InvalidProtocol";  // 无效协议
    
    // 验证配置
    bool isValid = config.validateConfig();
    std::cout << "配置是否有效: " << (isValid ? "是" : "否") << std::endl;
    
    if (!isValid) {
        auto errors = config.getValidationErrors();
        std::cout << "验证错误 (" << errors.size() << " 个):" << std::endl;
        for (const auto& error : errors) {
            std::cout << "  - " << error << std::endl;
        }
    }
    
    // 重置为默认配置
    config.setDefaults();
    std::cout << "\n已重置为默认配置" << std::endl;
}

void demonstrateFileOperations() {
    std::cout << "\n=== 文件操作示例 ===" << std::endl;
    
    auto& config = ClientConfigManager::getInstance();
    
    // 获取默认配置文件路径
    std::string configPath = config.getDefaultConfigPath();
    std::cout << "默认配置文件路径: " << configPath << std::endl;
    
    // 检查配置文件是否存在
    bool exists = config.configFileExists(configPath);
    std::cout << "配置文件是否存在: " << (exists ? "是" : "否") << std::endl;
    
    // 尝试保存配置（当前实现会返回false，因为JSON序列化尚未实现）
    bool saved = config.saveToFile(configPath);
    std::cout << "保存配置结果: " << (saved ? "成功" : "失败 (功能待实现)") << std::endl;
    
    // 尝试加载配置（当前实现会返回false，因为JSON反序列化尚未实现）
    bool loaded = config.loadFromFile(configPath);
    std::cout << "加载配置结果: " << (loaded ? "成功" : "失败 (功能待实现)") << std::endl;
}

void demonstrateConfigCallback() {
    std::cout << "\n=== 配置变更回调示例 ===" << std::endl;
    
    auto& config = ClientConfigManager::getInstance();
    
    // 设置配置变更回调函数
    config.setConfigChangeCallback([](const std::string& section, const std::string& key) {
        std::cout << "配置已变更 - 节: " << section << ", 键: " << key << std::endl;
    });
    
    // 模拟配置变更通知
    config.notifyConfigChanged("upload", "serverHost");
    config.notifyConfigChanged("ui", "language");
    config.notifyConfigChanged("network", "connectTimeoutMs");
}

void demonstrateAdvancedConfig() {
    std::cout << "\n=== 高级配置示例 ===" << std::endl;
    
    auto& config = ClientConfigManager::getInstance();
    
    // 配置上传相关设置
    auto& uploadConfig = config.getUploadConfig();
    uploadConfig.enableCompression = true;
    uploadConfig.enableChecksum = true;
    uploadConfig.maxUploadSpeed = 10 * 1024 * 1024;  // 10MB/s
    uploadConfig.maxFileSize = 100 * 1024 * 1024;   // 100MB
    uploadConfig.retryCount = 5;
    uploadConfig.retryDelayMs = 2000;
    
    // 添加排除模式
    uploadConfig.excludePatterns = {"*.tmp", "*.bak", "*.log", "thumbs.db"};
    
    // 配置SSL/TLS
    uploadConfig.useSSL = true;
    uploadConfig.certFile = "./certs/client.crt";
    uploadConfig.privateKeyFile = "./certs/client.key";
    uploadConfig.caFile = "./certs/ca.crt";
    uploadConfig.verifyServer = true;
    
    // 配置认证
    uploadConfig.authToken = "Bearer eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9...";
    
    // 配置日志
    uploadConfig.logLevel = "DEBUG";
    uploadConfig.logFilePath = "./logs/upload_client.log";
    uploadConfig.enableDetailedLog = true;
    
    // 配置网络设置
    auto& networkConfig = config.getNetworkConfig();
    networkConfig.enableProxy = true;
    networkConfig.proxyHost = "proxy.company.com";
    networkConfig.proxyPort = 8080;
    networkConfig.proxyUser = "username";
    networkConfig.proxyPassword = "password";
    
    std::cout << "已配置高级设置:" << std::endl;
    std::cout << "- 启用压缩和校验" << std::endl;
    std::cout << "- 限速: " << (uploadConfig.maxUploadSpeed / 1024 / 1024) << " MB/s" << std::endl;
    std::cout << "- 文件大小限制: " << (uploadConfig.maxFileSize / 1024 / 1024) << " MB" << std::endl;
    std::cout << "- SSL/TLS: " << (uploadConfig.useSSL ? "启用" : "禁用") << std::endl;
    std::cout << "- 代理服务器: " << (networkConfig.enableProxy ? networkConfig.proxyHost + ":" + std::to_string(networkConfig.proxyPort) : "未启用") << std::endl;
    std::cout << "- 排除模式数量: " << uploadConfig.excludePatterns.size() << std::endl;
}

int main() {
    try {
        demonstrateBasicUsage();
        demonstrateConfigValidation();
        demonstrateFileOperations();
        demonstrateConfigCallback();
        demonstrateAdvancedConfig();
        
        std::cout << "\n=== 配置管理器演示完成 ===" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}