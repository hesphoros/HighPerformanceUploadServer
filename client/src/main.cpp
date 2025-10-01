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

#include "Config/ClientConfigManager.h"


LightLogWrite_Impl configLogWriter;

void TestConfig() {
    std::cout << "\n=== ClientConfigManager 测试开始 ===" << std::endl;

    auto& configMgr = ClientConfigManager::getInstance();

    // 1. 测试默认配置
    std::cout << "\n1. 测试默认配置:" << std::endl;
    configMgr.setDefaults();

    auto& uploadConfig = configMgr.getUploadConfig();
    auto& uiConfig = configMgr.getUIConfig();
    auto& networkConfig = configMgr.getNetworkConfig();

    std::cout << "  服务器地址: " << uploadConfig.serverHost << ":" << uploadConfig.serverPort << std::endl;
    std::cout << "  压缩算法: " << uploadConfig.compressionAlgo << std::endl;
    std::cout << "  校验算法: " << uploadConfig.checksumAlgo << std::endl;
    std::cout << "  最大并发上传: " << uploadConfig.maxConcurrentUploads << std::endl;
    std::cout << "  分块大小: " << uploadConfig.chunkSize << " bytes" << std::endl;

    // 2. 测试配置修改
    std::cout << "\n2. 测试配置修改:" << std::endl;
    uploadConfig.serverHost = "192.168.1.100";
    uploadConfig.serverPort = 8080;
    uploadConfig.compressionAlgo = CompressionAlgorithm::GZIP;
    uploadConfig.checksumAlgo = ChecksumAlgorithm::MD5;
    uploadConfig.maxConcurrentUploads = 8;
    uploadConfig.chunkSize = 2 * 1024 * 1024; // 2MB
    uploadConfig.enableCompression = true;
    uploadConfig.enableChecksum = true;

    uiConfig.language = "en-US";
    uiConfig.theme = "dark";
    uiConfig.windowWidth = 1200;
    uiConfig.windowHeight = 800;

    networkConfig.connectTimeoutMs = 10000;
    networkConfig.bufferSize = 16384;
    networkConfig.enableProxy = true;
    networkConfig.proxyHost = "proxy.example.com";
    networkConfig.proxyPort = 3128;

    std::cout << "  修改后服务器地址: " << uploadConfig.serverHost << ":" << uploadConfig.serverPort << std::endl;
    std::cout << "  修改后压缩算法: " << uploadConfig.compressionAlgo << std::endl;
    std::cout << "  修改后校验算法: " << uploadConfig.checksumAlgo << std::endl;
    std::cout << "  修改后UI语言: " << uiConfig.language << std::endl;
    std::cout << "  修改后网络代理: " << networkConfig.proxyHost << ":" << networkConfig.proxyPort << std::endl;

    // 3. 测试配置验证
    std::cout << "\n3. 测试配置验证:" << std::endl;
    bool isValid = configMgr.validateConfig();
    std::cout << "  配置验证结果: " << (isValid ? "✅ 有效" : "❌ 无效") << std::endl;

    if (!isValid) {
        auto errors = configMgr.getValidationErrors();
        std::cout << "  验证错误:" << std::endl;
        for (const auto& error : errors) {
            std::cout << "    - " << error << std::endl;
        }
    }

    // 4. 测试TOML导出
    std::cout << "\n4. 测试TOML导出:" << std::endl;
    std::string tomlContent = configMgr.exportToTomlString();
    std::cout << "  导出的TOML内容 (前200字符):" << std::endl;
    std::cout << "  " << tomlContent.substr(0, 200) << "..." << std::endl;

    // 5. 测试保存到文件
    std::cout << "\n5. 测试保存配置到文件:" << std::endl;
    std::string testConfigPath = "./config/test_config.toml";
    bool saveSuccess = configMgr.saveToFile(testConfigPath);
    std::cout << "  保存到 " << testConfigPath << ": " << (saveSuccess ? "✅ 成功" : "❌ 失败") << std::endl;

    // 6. 测试重置配置
    std::cout << "\n6. 测试配置重置:" << std::endl;
    configMgr.setDefaults();  // 重置为默认值
    std::cout << "  重置后服务器地址: " << configMgr.getUploadConfig().serverHost << std::endl;
    std::cout << "  重置后压缩算法: " << configMgr.getUploadConfig().compressionAlgo << std::endl;

    // 7. 测试从文件加载
    std::cout << "\n7. 测试从文件加载配置:" << std::endl;
    bool loadSuccess = configMgr.loadFromFile(testConfigPath);
    std::cout << "  从 " << testConfigPath << " 加载: " << (loadSuccess ? "✅ 成功" : "❌ 失败") << std::endl;

    if (loadSuccess) {
        std::cout << "  加载后服务器地址: " << configMgr.getUploadConfig().serverHost << ":"
            << configMgr.getUploadConfig().serverPort << std::endl;
        std::cout << "  加载后压缩算法: " << configMgr.getUploadConfig().compressionAlgo << std::endl;
        std::cout << "  加载后UI语言: " << configMgr.getUIConfig().language << std::endl;
    }

    // 8. 测试TOML字符串导入
    std::cout << "\n8. 测试从TOML字符串导入:" << std::endl;
    std::string testTomlContent = R"(
[upload]
serverHost = "test.server.com"
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
    std::cout << "  从TOML字符串导入: " << (importSuccess ? "✅ 成功" : "❌ 失败") << std::endl;

    if (importSuccess) {
        std::cout << "  导入后服务器地址: " << configMgr.getUploadConfig().serverHost << ":"
            << configMgr.getUploadConfig().serverPort << std::endl;
        std::cout << "  导入后压缩算法: " << configMgr.getUploadConfig().compressionAlgo << std::endl;
        std::cout << "  导入后UI语言: " << configMgr.getUIConfig().language << std::endl;
        std::cout << "  导入后连接超时: " << configMgr.getNetworkConfig().connectTimeoutMs << "ms" << std::endl;
    }

    // 9. 测试配置摘要
    std::cout << "\n9. 配置摘要信息:" << std::endl;
    std::string summary = configMgr.getConfigSummary();
    std::cout << summary << std::endl;

    // 10. 测试枚举转换功能
    std::cout << "\n10. 测试枚举转换功能:" << std::endl;

    // 测试CompressionAlgorithm转换
    std::cout << "  CompressionAlgorithm 转换测试:" << std::endl;
    for (auto algo : { CompressionAlgorithm::NONE, CompressionAlgorithm::GZIP,
                      CompressionAlgorithm::ZSTD, CompressionAlgorithm::LZ4,
                      CompressionAlgorithm::BROTLI, CompressionAlgorithm::LZMA }) {
        std::cout << "    " << algo << " -> 字符串 -> 枚举测试: ";
        std::string str = std::string(CompressionAlgorithmToString(algo));
        auto converted = StringToCompressionAlgorithm(str);
        bool success = (converted.has_value() && converted.value() == algo);
        std::cout << (success ? "✅" : "❌") << " (" << str << ")" << std::endl;
    }

    // 测试ChecksumAlgorithm转换
    std::cout << "  ChecksumAlgorithm 转换测试:" << std::endl;
    for (auto algo : { ChecksumAlgorithm::NONE, ChecksumAlgorithm::CRC32,
                      ChecksumAlgorithm::MD5, ChecksumAlgorithm::SHA1,
                      ChecksumAlgorithm::SHA256, ChecksumAlgorithm::SHA512,
                      ChecksumAlgorithm::BLAKE2 }) {
        std::cout << "    " << algo << " -> 字符串 -> 枚举测试: ";
        std::string str = std::string(ChecksumAlgorithmToString(algo));
        auto converted = StringToChecksumAlgorithm(str);
        bool success = (converted.has_value() && converted.value() == algo);
        std::cout << (success ? "✅" : "❌") << " (" << str << ")" << std::endl;
    }

    // 11. 测试配置变更回调
    std::cout << "\n11. 测试配置变更回调:" << std::endl;
    configMgr.setConfigChangeCallback([](const std::string& section, const std::string& key) {
        std::cout << "  🔔 配置变更通知: [" << section << "] " << key << std::endl;
        });

    // 触发一些配置变更
    configMgr.notifyConfigChanged("upload", "serverHost");
    configMgr.notifyConfigChanged("ui", "theme");
    configMgr.notifyConfigChanged("network", "proxyHost");

    // 12. 测试默认配置文件创建
    std::cout << "\n12. 测试默认配置文件创建:" << std::endl;
    std::string defaultPath = "./config/default_test.toml";
    bool createSuccess = configMgr.createDefaultConfigFile(defaultPath);
    std::cout << "  创建默认配置文件 " << defaultPath << ": "
        << (createSuccess ? "✅ 成功" : "❌ 失败") << std::endl;

    std::cout << "\n=== ClientConfigManager 测试完成 ===" << std::endl;
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

    TestConfig();


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
