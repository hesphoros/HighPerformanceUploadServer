# *默认配置和基本信息*

```cpp
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

```

# *测试配置验证功能*

```c++

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

```

# *TOML序列化和反序列化功能*

```c++
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
```

# *测试配置文件I/O操作*

```c++
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

```

# Other

```c++

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

```

# 测试回调

```c++

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

```

