/**
 * @file magic_enum_config_example.cpp
 * @brief 展示如何在ClientConfigManager中使用magic_enum
 * @date 2025-10-01
 * 
 * 这个文件展示了如何使用magic_enum库来替换手工编写的枚举转换函数
 * 提供更安全、更简洁的枚举处理方式
 */

#include "Config/ClientConfigManager.h"
#include "magic_enum_integration.h"
#ifdef MAGIC_ENUM_AVAILABLE
#include <magic_enum.hpp>
#endif
#include <iostream>
#include <vector>

using namespace UploadClient;

/**
 * @brief 使用magic_enum增强的配置管理器示例
 */
class EnhancedClientConfigManager {
public:
    // 假设这些枚举定义在ClientConfigManager.h中
    // 这里重新定义用于示例
    enum class CompressionAlgorithm {
        NONE = 0,
        GZIP,
        ZSTD, 
        LZ4,
        BROTLI,
        LZMA
    };

    enum class ChecksumAlgorithm {
        NONE = 0,
        CRC32,
        MD5,
        SHA1,
        SHA256,
        SHA512,
        BLAKE2
    };

    /**
     * @brief 使用magic_enum的现代化枚举转换
     */
    std::string compressionAlgorithmToString(CompressionAlgorithm algo) const {
#ifdef MAGIC_ENUM_AVAILABLE
        // 一行代码替换整个switch语句
        return std::string(magic_enum::enum_name(algo));
#else
        // 降级到传统方式
        switch (algo) {
            case CompressionAlgorithm::NONE: return "NONE";
            case CompressionAlgorithm::GZIP: return "GZIP";
            case CompressionAlgorithm::ZSTD: return "ZSTD";
            case CompressionAlgorithm::LZ4: return "LZ4";
            case CompressionAlgorithm::BROTLI: return "BROTLI";
            case CompressionAlgorithm::LZMA: return "LZMA";
            default: return "UNKNOWN";
        }
#endif
    }

    CompressionAlgorithm compressionAlgorithmFromString(const std::string& str) const {
#ifdef MAGIC_ENUM_AVAILABLE
        // 类型安全的转换，无需手动比较字符串
        auto result = magic_enum::enum_cast<CompressionAlgorithm>(str);
        return result.value_or(CompressionAlgorithm::NONE);
#else
        // 传统方式
        if (str == "NONE") return CompressionAlgorithm::NONE;
        if (str == "GZIP") return CompressionAlgorithm::GZIP;
        if (str == "ZSTD") return CompressionAlgorithm::ZSTD;
        if (str == "LZ4") return CompressionAlgorithm::LZ4;
        if (str == "BROTLI") return CompressionAlgorithm::BROTLI;
        if (str == "LZMA") return CompressionAlgorithm::LZMA;
        return CompressionAlgorithm::NONE;
#endif
    }

    std::string checksumAlgorithmToString(ChecksumAlgorithm algo) const {
#ifdef MAGIC_ENUM_AVAILABLE
        return std::string(magic_enum::enum_name(algo));
#else
        switch (algo) {
            case ChecksumAlgorithm::NONE: return "NONE";
            case ChecksumAlgorithm::CRC32: return "CRC32";
            case ChecksumAlgorithm::MD5: return "MD5";
            case ChecksumAlgorithm::SHA1: return "SHA1";
            case ChecksumAlgorithm::SHA256: return "SHA256";
            case ChecksumAlgorithm::SHA512: return "SHA512";
            case ChecksumAlgorithm::BLAKE2: return "BLAKE2";
            default: return "UNKNOWN";
        }
#endif
    }

    ChecksumAlgorithm checksumAlgorithmFromString(const std::string& str) const {
#ifdef MAGIC_ENUM_AVAILABLE
        auto result = magic_enum::enum_cast<ChecksumAlgorithm>(str);
        return result.value_or(ChecksumAlgorithm::NONE);
#else
        if (str == "NONE") return ChecksumAlgorithm::NONE;
        if (str == "CRC32") return ChecksumAlgorithm::CRC32;
        if (str == "MD5") return ChecksumAlgorithm::MD5;
        if (str == "SHA1") return ChecksumAlgorithm::SHA1;
        if (str == "SHA256") return ChecksumAlgorithm::SHA256;
        if (str == "SHA512") return ChecksumAlgorithm::SHA512;
        if (str == "BLAKE2") return ChecksumAlgorithm::BLAKE2;
        return ChecksumAlgorithm::NONE;
#endif
    }

    /**
     * @brief 获取所有可用的压缩算法选项
     * @return 包含所有选项的向量，用于UI下拉列表
     */
    std::vector<std::string> getAvailableCompressionAlgorithms() const {
#ifdef MAGIC_ENUM_AVAILABLE
        auto names = magic_enum::enum_names<CompressionAlgorithm>();
        return std::vector<std::string>(names.begin(), names.end());
#else
        return {"NONE", "GZIP", "ZSTD", "LZ4", "BROTLI", "LZMA"};
#endif
    }

    /**
     * @brief 获取所有可用的校验算法选项
     */
    std::vector<std::string> getAvailableChecksumAlgorithms() const {
#ifdef MAGIC_ENUM_AVAILABLE
        auto names = magic_enum::enum_names<ChecksumAlgorithm>();
        return std::vector<std::string>(names.begin(), names.end());
#else
        return {"NONE", "CRC32", "MD5", "SHA1", "SHA256", "SHA512", "BLAKE2"};
#endif
    }

    /**
     * @brief 验证压缩算法配置
     * @param algorithmStr 算法字符串
     * @return 验证是否通过
     */
    bool validateCompressionAlgorithm(const std::string& algorithmStr) const {
#ifdef MAGIC_ENUM_AVAILABLE
        return magic_enum::enum_cast<CompressionAlgorithm>(algorithmStr).has_value();
#else
        auto valid_algorithms = getAvailableCompressionAlgorithms();
        return std::find(valid_algorithms.begin(), valid_algorithms.end(), algorithmStr) != valid_algorithms.end();
#endif
    }

    /**
     * @brief 增强的TOML解析，带有详细的枚举验证
     */
    bool parseEnhancedTomlConfig(const std::string& tomlContent) {
        std::cout << "=== 增强版TOML配置解析 ===" << std::endl;
        
#ifdef TOML11_AVAILABLE && MAGIC_ENUM_AVAILABLE
        try {
            auto data = toml::parse_str(tomlContent);
            
            if (data.contains("upload")) {
                auto upload = data["upload"];
                
                // 压缩算法解析与验证
                if (upload.contains("compression_algorithm")) {
                    std::string algo_str = upload["compression_algorithm"].as_string();
                    std::cout << "配置的压缩算法: " << algo_str << std::endl;
                    
                    auto algo = magic_enum::enum_cast<CompressionAlgorithm>(algo_str);
                    if (algo.has_value()) {
                        std::cout << "✅ 压缩算法有效: " << magic_enum::enum_name(algo.value()) << std::endl;
                        
                        // 获取算法的数值
                        std::cout << "   算法ID: " << static_cast<int>(algo.value()) << std::endl;
                    } else {
                        std::cout << "❌ 无效的压缩算法: " << algo_str << std::endl;
                        std::cout << "   可用选项: ";
                        auto valid_options = magic_enum::enum_names<CompressionAlgorithm>();
                        for (size_t i = 0; i < valid_options.size(); ++i) {
                            if (i > 0) std::cout << ", ";
                            std::cout << valid_options[i];
                        }
                        std::cout << std::endl;
                        return false;
                    }
                }

                // 校验算法解析与验证
                if (upload.contains("checksum_algorithm")) {
                    std::string checksum_str = upload["checksum_algorithm"].as_string();
                    std::cout << "配置的校验算法: " << checksum_str << std::endl;
                    
                    auto checksum = magic_enum::enum_cast<ChecksumAlgorithm>(checksum_str);
                    if (checksum.has_value()) {
                        std::cout << "✅ 校验算法有效: " << magic_enum::enum_name(checksum.value()) << std::endl;
                    } else {
                        std::cout << "❌ 无效的校验算法: " << checksum_str << std::endl;
                        return false;
                    }
                }
            }
            
            std::cout << "✅ TOML配置解析成功" << std::endl;
            return true;
        }
        catch (const std::exception& e) {
            std::cout << "❌ TOML解析异常: " << e.what() << std::endl;
            return false;
        }
#else
        std::cout << "⚠️ TOML11或magic_enum支持未启用" << std::endl;
        return false;
#endif
    }

    /**
     * @brief 打印所有支持的枚举选项
     */
    void printSupportedOptions() const {
        std::cout << "\n=== 支持的配置选项 ===" << std::endl;
        
#ifdef MAGIC_ENUM_AVAILABLE
        std::cout << "压缩算法 (" << magic_enum::enum_count<CompressionAlgorithm>() << " 个选项):" << std::endl;
        for (auto algo : magic_enum::enum_values<CompressionAlgorithm>()) {
            std::cout << "  - " << magic_enum::enum_name(algo) 
                      << " (ID: " << static_cast<int>(algo) << ")" << std::endl;
        }
        
        std::cout << "\n校验算法 (" << magic_enum::enum_count<ChecksumAlgorithm>() << " 个选项):" << std::endl;
        for (auto algo : magic_enum::enum_values<ChecksumAlgorithm>()) {
            std::cout << "  - " << magic_enum::enum_name(algo) 
                      << " (ID: " << static_cast<int>(algo) << ")" << std::endl;
        }
#else
        std::cout << "magic_enum支持未启用，无法自动枚举选项" << std::endl;
#endif
    }
};

/**
 * @brief magic_enum功能演示
 */
void demonstrateMagicEnumFeatures() {
    std::cout << "\n=== magic_enum功能演示 ===" << std::endl;
    
    EnhancedClientConfigManager config;
    
    // 1. 基础转换测试
    std::cout << "\n1. 枚举转换测试:" << std::endl;
    auto compression = EnhancedClientConfigManager::CompressionAlgorithm::GZIP;
    std::cout << "枚举值 -> 字符串: " << config.compressionAlgorithmToString(compression) << std::endl;
    
    auto parsed = config.compressionAlgorithmFromString("LZ4");
    std::cout << "字符串 -> 枚举值: " << config.compressionAlgorithmToString(parsed) << std::endl;
    
    // 2. 验证功能测试
    std::cout << "\n2. 验证功能测试:" << std::endl;
    std::vector<std::string> test_values = {"GZIP", "INVALID_ALGO", "LZ4", "UNKNOWN"};
    for (const auto& value : test_values) {
        bool valid = config.validateCompressionAlgorithm(value);
        std::cout << "'" << value << "' -> " << (valid ? "✅ 有效" : "❌ 无效") << std::endl;
    }
    
    // 3. 获取所有选项
    std::cout << "\n3. 所有可用选项:" << std::endl;
    auto compression_options = config.getAvailableCompressionAlgorithms();
    std::cout << "压缩算法选项: ";
    for (size_t i = 0; i < compression_options.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << compression_options[i];
    }
    std::cout << std::endl;
    
    // 4. 打印详细信息
    config.printSupportedOptions();
    
    // 5. TOML解析测试
    std::cout << "\n4. TOML解析测试:" << std::endl;
    std::string test_toml = R"(
[upload]
compression_algorithm = "GZIP"
checksum_algorithm = "SHA256"
)";
    config.parseEnhancedTomlConfig(test_toml);
    
    std::string invalid_toml = R"(
[upload]
compression_algorithm = "INVALID"
checksum_algorithm = "MD5"
)";
    std::cout << "\n测试无效配置:" << std::endl;
    config.parseEnhancedTomlConfig(invalid_toml);
}

/**
 * @brief 如何集成到现有ClientConfigManager的建议
 */
void integrationSuggestions() {
    std::cout << "\n=== 集成建议 ===" << std::endl;
    std::cout << "1. 在ClientConfigManager.h中添加:" << std::endl;
    std::cout << "   #include \"magic_enum_integration.h\"" << std::endl;
    std::cout << "   #ifdef MAGIC_ENUM_AVAILABLE" << std::endl;
    std::cout << "   #include <magic_enum.hpp>" << std::endl;
    std::cout << "   #endif" << std::endl;
    
    std::cout << "\n2. 替换现有的枚举转换函数:" << std::endl;
    std::cout << "   使用magic_enum::enum_name()替换switch语句" << std::endl;
    std::cout << "   使用magic_enum::enum_cast()替换字符串比较" << std::endl;
    
    std::cout << "\n3. 添加新功能:" << std::endl;
    std::cout << "   - 枚举验证函数" << std::endl;
    std::cout << "   - 获取所有选项的函数（用于UI）" << std::endl;
    std::cout << "   - 更好的错误提示" << std::endl;
    
    std::cout << "\n4. 保持向后兼容:" << std::endl;
    std::cout << "   使用条件编译保证在没有magic_enum时仍能工作" << std::endl;
}

/**
 * @brief 主演示函数
 */
int main() {
    std::cout << "=== ClientConfigManager magic_enum集成演示 ===" << std::endl;
    
#ifdef MAGIC_ENUM_AVAILABLE
    std::cout << "✅ magic_enum库已启用" << std::endl;
#else
    std::cout << "⚠️ magic_enum库未启用，将使用传统方式" << std::endl;
#endif

#ifdef TOML11_AVAILABLE
    std::cout << "✅ TOML11库已启用" << std::endl;
#else
    std::cout << "⚠️ TOML11库未启用" << std::endl;
#endif
    
    demonstrateMagicEnumFeatures();
    integrationSuggestions();
    
    return 0;
}