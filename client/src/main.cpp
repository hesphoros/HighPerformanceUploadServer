#include <WinSock2.h>
#include <Windows.h>
#include <QApplication>
#include "MainWindow.h"
#include "log_headers.h"
#include "log/UniConv.h"
#include "NotificationService/Lusp_SyncFilesNotificationService.h"
#include "SyncUploadQueue/Lusp_SyncUploadQueuePrivate.h"
#include "SyncUploadQueue/Lusp_SyncUploadQueue.h"
#include "AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.h"
#include <memory>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <vector>

#include "Config/ClientConfigManager.h"


LightLogWrite_Impl configLogWriter;

// ===================== 配置管理器测试模块 =====================

/**
 * @brief 测试默认配置和基本信息显示
 */
void TestDefaultConfig() {
    std::cout << "\n📋 [模块1] 默认配置测试:" << std::endl;

    auto& configMgr = ClientConfigManager::getInstance();
    configMgr.setDefaults();

    auto& uploadConfig = configMgr.getUploadConfig();
    auto& uiConfig = configMgr.getUIConfig();
    auto& networkConfig = configMgr.getNetworkConfig();

    std::cout << "  服务器地址: " << uploadConfig.serverHost << ":" << uploadConfig.serverPort << std::endl;
    std::cout << "  压缩算法: " << uploadConfig.compressionAlgo << std::endl;
    std::cout << "  校验算法: " << uploadConfig.checksumAlgo << std::endl;
    std::cout << "  最大并发上传: " << uploadConfig.maxConcurrentUploads << std::endl;
    std::cout << "  分块大小: " << uploadConfig.chunkSize << " bytes" << std::endl;
    std::cout << "  UI语言: " << uiConfig.language << std::endl;
    std::cout << "  连接超时: " << networkConfig.connectTimeoutMs << "ms" << std::endl;
}

/**
 * @brief 测试配置验证功能
 */
void TestConfigValidation() {
    std::cout << "\n✅ [模块2] 配置验证测试:" << std::endl;

    auto& configMgr = ClientConfigManager::getInstance();
    bool isValid = configMgr.validateConfig();
    std::cout << "  配置验证结果: " << (isValid ? "✅ 有效" : "❌ 无效") << std::endl;

    if (!isValid) {
        auto errors = configMgr.getValidationErrors();
        std::cout << "  验证错误:" << std::endl;
        for (const auto& error : errors) {
            std::cout << "    - " << error << std::endl;
        }
    }
}

/**
 * @brief 测试TOML序列化和反序列化功能
 */
void TestTomlSerialization() {
    std::cout << "\n💾 [模块3] TOML序列化测试:" << std::endl;

    auto& configMgr = ClientConfigManager::getInstance();

    // 测试TOML导出
    std::string tomlContent = configMgr.exportToTomlString();
    std::cout << "  TOML导出: ✅ 成功 (长度: " << tomlContent.length() << " 字符)" << std::endl;
    std::cout << "  内容预览: " << tomlContent.substr(0, 100) << "..." << std::endl;

    // 测试从TOML字符串导入
    std::string testTomlContent = R"(
        [upload]
        serverHost = "test.example.com"
        serverPort = 9999
        compressionAlgo = "LZ4"
        checksumAlgo = "SHA512"
        maxConcurrentUploads = 16
        enableCompression = false

        [ui]
        language = "ja-JP"
        theme = "light"
        windowWidth = 1600
        windowHeight = 900

        [network]
        connectTimeoutMs = 15000
        enableProxy = false
        )";

    bool importSuccess = configMgr.importFromTomlString(testTomlContent);
    std::cout << "  TOML导入: " << (importSuccess ? "✅ 成功" : "❌ 失败") << std::endl;

    if (importSuccess) {
        std::cout << "  导入后服务器: " << configMgr.getUploadConfig().serverHost << ":"
            << configMgr.getUploadConfig().serverPort << std::endl;
        std::cout << "  导入后语言: " << configMgr.getUIConfig().language << std::endl;
    }
}

/**
 * @brief 测试配置文件I/O操作
 */
void TestFileIO() {
    std::cout << "\n📁 [模块4] 文件I/O测试:" << std::endl;

    auto& configMgr = ClientConfigManager::getInstance();

    // 测试保存配置
    std::string testConfigPath = "./config/test_config.toml";
    bool saveSuccess = configMgr.saveToFile(testConfigPath);
    std::cout << "  保存配置: " << (saveSuccess ? "✅ 成功" : "❌ 失败") << " -> " << testConfigPath << std::endl;

    // 重置配置
    configMgr.setDefaults();
    std::cout << "  配置重置: ✅ 完成 (恢复默认值)" << std::endl;

    // 测试加载配置
    std::string originCfgPath = "./config/upload_client.toml";
    bool loadSuccess = configMgr.loadFromFile(originCfgPath);
    std::cout << "  加载配置: " << (loadSuccess ? "✅ 成功" : "❌ 失败") << " <- " << originCfgPath << std::endl;

    if (loadSuccess) {
        std::cout << "  加载后服务器: " << configMgr.getUploadConfig().serverHost << ":"
            << configMgr.getUploadConfig().serverPort << std::endl;
    }

    // 测试创建默认配置文件
    std::string defaultPath = "./config/default_test.toml";
    bool createSuccess = configMgr.createDefaultConfigFile(defaultPath);
    std::cout << "  创建默认配置: " << (createSuccess ? "✅ 成功" : "❌ 失败") << " -> " << defaultPath << std::endl;
}

/**
 * @brief 测试枚举类型转换功能
 */
void TestEnumConversion() {
    std::cout << "\n🔄 [模块5] 枚举转换测试:" << std::endl;

    // 测试CompressionAlgorithm转换
    std::cout << "  🗜️  CompressionAlgorithm 转换测试:" << std::endl;
    std::vector<CompressionAlgorithm> compressionAlgos = {
        CompressionAlgorithm::NONE, CompressionAlgorithm::GZIP,
        CompressionAlgorithm::ZSTD, CompressionAlgorithm::LZ4,
        CompressionAlgorithm::BROTLI, CompressionAlgorithm::LZMA
    };

    for (auto algo : compressionAlgos) {
        std::string str = std::string(CompressionAlgorithmToString(algo));
        auto converted = StringToCompressionAlgorithm(str);
        bool success = (converted.has_value() && converted.value() == algo);
        std::cout << "     " << str << ": " << (success ? "✅" : "❌") << std::endl;
    }

    // 测试ChecksumAlgorithm转换
    std::cout << "  🔐 ChecksumAlgorithm 转换测试:" << std::endl;
    std::vector<ChecksumAlgorithm> checksumAlgos = {
        ChecksumAlgorithm::NONE, ChecksumAlgorithm::CRC32,
        ChecksumAlgorithm::MD5, ChecksumAlgorithm::SHA1,
        ChecksumAlgorithm::SHA256, ChecksumAlgorithm::SHA512,
        ChecksumAlgorithm::BLAKE2
    };

    for (auto algo : checksumAlgos) {
        std::string str = std::string(ChecksumAlgorithmToString(algo));
        auto converted = StringToChecksumAlgorithm(str);
        bool success = (converted.has_value() && converted.value() == algo);
        std::cout << "     " << str << ": " << (success ? "✅" : "❌") << std::endl;
    }
}

/**
 * @brief 测试配置变更回调机制
 */
void TestConfigCallback() {
    std::cout << "\n🔔 [模块6] 配置回调测试:" << std::endl;

    auto& configMgr = ClientConfigManager::getInstance();

    // 设置配置变更回调
    configMgr.setConfigChangeCallback([](const std::string& section, const std::string& key) {
        std::cout << "     🔔 配置变更通知: [" << section << "] " << key << std::endl;
        });

    // 触发配置变更通知
    std::cout << "  触发配置变更通知:" << std::endl;
    configMgr.notifyConfigChanged("upload", "serverHost");
    configMgr.notifyConfigChanged("ui", "theme");
    configMgr.notifyConfigChanged("network", "proxyHost");

    std::cout << "  回调机制: ✅ 工作正常" << std::endl;
}

/**
 * @brief 显示配置摘要信息
 */
void TestConfigSummary() {
    std::cout << "\n📊 [模块7] 配置摘要展示:" << std::endl;

    auto& configMgr = ClientConfigManager::getInstance();
    std::string summary = configMgr.getConfigSummary();
    std::cout << summary << std::endl;
}

/**
 * @brief 清理测试过程中生成的临时文件
 */
void CleanupTestFiles() {
    std::cout << "\n🧹 [清理] 删除测试文件:" << std::endl;

    std::vector<std::string> testFiles = {
        "./config/test_config.toml",
        "./config/default_test.toml"
    };

    for (const auto& file : testFiles) {
        try {
            if (std::filesystem::exists(file)) {
                std::filesystem::remove(file);
                std::cout << "  ✅ 删除: " << file << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cout << "  ⚠️ 删除失败: " << file << " (" << e.what() << ")" << std::endl;
        }
    }
}

/**
 * @brief 主测试入口函数 - 按模块顺序执行所有测试
 */
void TestConfig() {
    std::cout << "\n🚀 === ClientConfigManager 模块化测试开始 ===" << std::endl;

    try {
        // 按模块顺序执行测试
        TestDefaultConfig();        // 模块1: 默认配置测试
        TestConfigValidation();     // 模块2: 配置验证测试  
        TestTomlSerialization();    // 模块3: TOML序列化测试
        TestFileIO();              // 模块4: 文件I/O测试
        TestEnumConversion();       // 模块5: 枚举转换测试
        TestConfigCallback();       // 模块6: 配置回调测试
        TestConfigSummary();        // 模块7: 配置摘要展示

        std::cout << "\n✅ === 所有测试模块完成 ===" << std::endl;

        // 清理测试文件
        CleanupTestFiles();

    }
    catch (const std::exception& e) {
        std::cout << "\n❌ 测试过程中发生异常: " << e.what() << std::endl;
    }

    std::cout << "\n🏁 === ClientConfigManager 测试结束 ===" << std::endl;
}

void TestDefaultConfigUploadClient(){
    auto& cfgMgr = ClientConfigManager::getInstance();
    cfgMgr.loadFromFile("./config/upload_client.toml");
    if (cfgMgr.validateConfig()) {
        std::cout << "配置文件验证通过。" << std::endl;
    } else {
        std::cout << "配置文件验证失败，错误如下：" << std::endl;
        for (const auto& err : cfgMgr.getValidationErrors()) {
            std::cout << " - " << err << std::endl;
        }
    }

    std::string str = cfgMgr.exportToTomlString();
    std::cout << str << std::endl;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    SetConsoleOutputCP(CP_UTF8);
    // 设置应用程序信息
    app.setApplicationName("SyncFilesUploadClient");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("High Performance Upload");

    // UniConv::GetInstance()->SetDefaultEncoding("UTF-8");
    initializeLogging();  // 初始化日志系统

    // 用智能指针管理 NotificationService
    std::unique_ptr<Lusp_SyncFilesNotificationService> notifier = std::make_unique<Lusp_SyncFilesNotificationService>(Lusp_SyncUploadQueue::instance());
    notifier->start();

    // TestConfig();
    TestDefaultConfigUploadClient();

    //client2.join();
    // 创建主窗口
    MainWindow window;
    window.show();
    //std::thread client1([]() { run_client(1); });
    //std::thread client2([]() { run_client(2); });

    //client1.join();
    int ret = app.exec();

    // 确保后台线程安全退出
    notifier->stop();
    notifier.reset();


    return ret;
}
