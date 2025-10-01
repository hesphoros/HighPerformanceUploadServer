/**
 * @file magic_enum_integration.h
 * @brief magic_enum库集成示例和工具
 * @author HighPerformanceUploadClient Team
 * @date 2025-10-01
 * 
 * magic_enum是一个现代C++库，提供静态反射功能用于枚举类型
 * 支持枚举到字符串的转换、字符串到枚举的转换、枚举迭代等功能
 */

#ifndef MAGIC_ENUM_INTEGRATION_H
#define MAGIC_ENUM_INTEGRATION_H

#ifdef MAGIC_ENUM_AVAILABLE
#include <magic_enum.hpp>
#include <magic_enum_format.hpp>
#include <magic_enum_iostream.hpp>
#include <magic_enum_containers.hpp>
#endif

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <iostream>

namespace UploadClient {

/**
 * @brief 增强版枚举工具类
 * 
 * 使用magic_enum库提供类型安全的枚举操作
 */
class EnhancedEnumUtils {
public:
    /**
     * @brief 枚举到字符串转换（类型安全）
     * @tparam E 枚举类型
     * @param value 枚举值
     * @return 字符串表示，如果转换失败返回空字符串
     */
    template<typename E>
    static std::string ToString(E value) {
#ifdef MAGIC_ENUM_AVAILABLE
        return std::string(magic_enum::enum_name(value));
#else
        return "MAGIC_ENUM_NOT_AVAILABLE";
#endif
    }

    /**
     * @brief 字符串到枚举转换（类型安全）
     * @tparam E 枚举类型
     * @param str 字符串表示
     * @return 枚举值的optional，如果转换失败返回nullopt
     */
    template<typename E>
    static std::optional<E> FromString(std::string_view str) {
#ifdef MAGIC_ENUM_AVAILABLE
        return magic_enum::enum_cast<E>(str);
#else
        return std::nullopt;
#endif
    }

    /**
     * @brief 获取枚举的所有可能值
     * @tparam E 枚举类型
     * @return 包含所有枚举值的向量
     */
    template<typename E>
    static std::vector<E> GetAllValues() {
#ifdef MAGIC_ENUM_AVAILABLE
        auto values = magic_enum::enum_values<E>();
        return std::vector<E>(values.begin(), values.end());
#else
        return {};
#endif
    }

    /**
     * @brief 获取枚举的所有名称
     * @tparam E 枚举类型
     * @return 包含所有枚举名称的向量
     */
    template<typename E>
    static std::vector<std::string> GetAllNames() {
#ifdef MAGIC_ENUM_AVAILABLE
        auto names = magic_enum::enum_names<E>();
        std::vector<std::string> result;
        result.reserve(names.size());
        for (const auto& name : names) {
            result.emplace_back(name);
        }
        return result;
#else
        return {};
#endif
    }

    /**
     * @brief 检查字符串是否为有效的枚举名称
     * @tparam E 枚举类型
     * @param str 字符串
     * @return 是否为有效的枚举名称
     */
    template<typename E>
    static bool IsValidName(std::string_view str) {
#ifdef MAGIC_ENUM_AVAILABLE
        return magic_enum::enum_cast<E>(str).has_value();
#else
        return false;
#endif
    }

    /**
     * @brief 获取枚举值的数量
     * @tparam E 枚举类型
     * @return 枚举值的数量
     */
    template<typename E>
    static constexpr std::size_t GetCount() {
#ifdef MAGIC_ENUM_AVAILABLE
        return magic_enum::enum_count<E>();
#else
        return 0;
#endif
    }

    /**
     * @brief 格式化枚举值为友好的显示名称
     * @tparam E 枚举类型
     * @param value 枚举值
     * @return 格式化后的字符串
     */
    template<typename E>
    static std::string ToDisplayName(E value) {
#ifdef MAGIC_ENUM_AVAILABLE
        std::string name = ToString(value);
        if (name.empty()) return "Unknown";
        
        // 将下划线替换为空格，首字母大写
        std::string result;
        bool capitalize_next = true;
        
        for (char c : name) {
            if (c == '_') {
                result += ' ';
                capitalize_next = true;
            } else if (capitalize_next) {
                result += std::toupper(c);
                capitalize_next = false;
            } else {
                result += std::tolower(c);
            }
        }
        
        return result;
#else
        return "MAGIC_ENUM_NOT_AVAILABLE";
#endif
    }
};

/**
 * @brief 增强版配置枚举转换器
 * 专门为ClientConfigManager中的枚举类型提供转换功能
 */
class ConfigEnumConverter {
public:
    /**
     * @brief 压缩算法枚举（假设定义在Config中）
     */
    enum class CompressionAlgorithm {
        NONE = 0,
        GZIP,
        ZSTD,
        LZ4,
        BROTLI,
        LZMA
    };

    /**
     * @brief 校验算法枚举（假设定义在Config中）
     */
    enum class ChecksumAlgorithm {
        NONE = 0,
        CRC32,
        MD5,
        SHA1,
        SHA256,
        SHA512,
        BLAKE2
    };

    // 使用magic_enum的转换函数
    static std::string CompressionToString(CompressionAlgorithm algo) {
        return EnhancedEnumUtils::ToString(algo);
    }

    static std::optional<CompressionAlgorithm> CompressionFromString(std::string_view str) {
        return EnhancedEnumUtils::FromString<CompressionAlgorithm>(str);
    }

    static std::string ChecksumToString(ChecksumAlgorithm algo) {
        return EnhancedEnumUtils::ToString(algo);
    }

    static std::optional<ChecksumAlgorithm> ChecksumFromString(std::string_view str) {
        return EnhancedEnumUtils::FromString<ChecksumAlgorithm>(str);
    }

    // 获取所有可用选项（用于UI下拉列表等）
    static std::vector<std::string> GetAllCompressionOptions() {
        return EnhancedEnumUtils::GetAllNames<CompressionAlgorithm>();
    }

    static std::vector<std::string> GetAllChecksumOptions() {
        return EnhancedEnumUtils::GetAllNames<ChecksumAlgorithm>();
    }
};

/**
 * @brief 枚举验证工具
 */
class EnumValidator {
public:
    /**
     * @brief 验证配置值是否有效
     * @tparam E 枚举类型
     * @param config_value 配置字符串值
     * @param default_value 默认值
     * @return 验证后的枚举值
     */
    template<typename E>
    static E ValidateOrDefault(const std::string& config_value, E default_value) {
        auto result = EnhancedEnumUtils::FromString<E>(config_value);
        if (result.has_value()) {
            return result.value();
        }
        
        // 记录警告日志
        std::cerr << "无效的枚举值: " << config_value 
                  << ", 使用默认值: " << EnhancedEnumUtils::ToString(default_value) << std::endl;
        
        return default_value;
    }

    /**
     * @brief 获取枚举的帮助信息
     * @tparam E 枚举类型
     * @return 包含所有有效选项的帮助字符串
     */
    template<typename E>
    static std::string GetHelpText() {
        auto names = EnhancedEnumUtils::GetAllNames<E>();
        std::string help = "有效选项: ";
        
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) help += ", ";
            help += names[i];
        }
        
        return help;
    }
};

} // namespace UploadClient

/**
 * @brief 便捷宏定义
 */
#ifdef MAGIC_ENUM_AVAILABLE
#define ENUM_TO_STRING(enum_value) magic_enum::enum_name(enum_value)
#define STRING_TO_ENUM(enum_type, str) magic_enum::enum_cast<enum_type>(str)
#define ENUM_COUNT(enum_type) magic_enum::enum_count<enum_type>()
#define FOR_EACH_ENUM(enum_type, var) for (auto var : magic_enum::enum_values<enum_type>())
#else
#define ENUM_TO_STRING(enum_value) "MAGIC_ENUM_NOT_AVAILABLE"
#define STRING_TO_ENUM(enum_type, str) std::nullopt
#define ENUM_COUNT(enum_type) 0
#define FOR_EACH_ENUM(enum_type, var) for (auto var : std::vector<enum_type>{})
#endif

/**
 * @brief 使用示例
 * 
 * ```cpp
 * #include "magic_enum_integration.h"
 * 
 * // 1. 基础转换
 * auto compression = CompressionAlgorithm::GZIP;
 * std::string name = EnhancedEnumUtils::ToString(compression);  // "GZIP"
 * auto parsed = EnhancedEnumUtils::FromString<CompressionAlgorithm>("LZ4");
 * 
 * // 2. 验证和默认值
 * auto validated = EnumValidator::ValidateOrDefault(
 *     config_string, CompressionAlgorithm::NONE);
 * 
 * // 3. 获取所有选项（UI使用）
 * auto options = EnhancedEnumUtils::GetAllNames<CompressionAlgorithm>();
 * for (const auto& option : options) {
 *     std::cout << "可选项: " << option << std::endl;
 * }
 * 
 * // 4. 使用便捷宏
 * FOR_EACH_ENUM(CompressionAlgorithm, algo) {
 *     std::cout << "算法: " << ENUM_TO_STRING(algo) << std::endl;
 * }
 * 
 * // 5. 友好显示
 * std::cout << EnhancedEnumUtils::ToDisplayName(CompressionAlgorithm::GZIP);  // "Gzip"
 * ```
 */

#endif // MAGIC_ENUM_INTEGRATION_H