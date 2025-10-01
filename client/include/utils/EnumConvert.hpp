#ifndef ENUM_CONVERT_HPP
#define ENUM_CONVERT_HPP

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <iostream>

#include <magic_enum.hpp>

namespace utils {

    template<typename E>
    class EnumConvert {
        static_assert(std::is_enum_v<E>, "Template parameter E must be an enum type.");
    public:
        /**
         * @brief 枚举值转换为字符串
         *
         * @param enumValue
         * @return constexpr std::string_view
         */
        static constexpr std::string_view toString(E enumValue) {
            return magic_enum::enum_name(enumValue);
        }

        /**
         * @brief 字符转换为枚举值
         *
         * @param str
         * @return constexpr std::optional<E>
         */
        static constexpr std::optional<E> fromString(const std::string& str) {
            return magic_enum::enum_cast<E>(str);
        }

        /**
         * @brief 字符串转换为枚举值（带默认值）
         *
         * @param str
         * @param default_value
         * @return constexpr E
         */
        static constexpr E fromStringOrDefault(const std::string_view str, const E default_value) noexcept {
            auto result = magic_enum::enum_cast<E>(str);
            return result.has_value() ? result.value() : default_value;
        }

        /**
         * @brief 获取所有枚举值
         *
         * @return constexpr auto
        */
        static constexpr auto getAllEnumValues() {
            return magic_enum::enum_values<E>();
        }

        /**
         * @brief 获取所有枚举名称
         *
         * @return constexpr auto
         */
        static constexpr auto getAllEnumNames() {
            return magic_enum::enum_names<E>();
        }
    };

    // 便利函数模板
    template<typename E>
    constexpr std::string_view enum_to_string(E value) noexcept {
        return EnumConvert<E>::toString(value);
    }

    template<typename E>
    constexpr std::optional<E> string_to_enum(const std::string& str) noexcept {
        return EnumConvert<E>::fromString(str);
    }

    template<typename E>
    constexpr E string_to_enum_or_default(std::string_view str, E default_value) noexcept {
        return EnumConvert<E>::fromStringOrDefault(str, default_value);
    }
};

// =========================== 宏定义区域 ===========================

/**
 * @brief 为指定枚举类型定义转换器类型别名
 * @param EnumType 枚举类型名称
 *
 * 使用示例:
 * DEFINE_ENUM_CONVERTER(Status)
 * // 生成: using StatusConverter = utils::EnumConvert<Status>;
 */
#define DEFINE_ENUM_CONVERTER(EnumType) \
    using EnumType##Converter = utils::EnumConvert<EnumType>

 /**
  * @brief 为指定枚举类型定义完整的转换器和便利函数
  * @param EnumType 枚举类型名称
  *
  * 使用示例:
  * DEFINE_ENUM_CONVERTER_FULL(Status)
  * // 生成转换器类型别名和所有便利函数
  */
#define DEFINE_ENUM_CONVERTER_FULL(EnumType) \
    DEFINE_ENUM_CONVERTER(EnumType); \
    inline constexpr std::string_view EnumType##ToString(EnumType value) noexcept { \
        return utils::enum_to_string(value); \
    } \
    inline constexpr std::optional<EnumType> StringTo##EnumType(const std::string& str) noexcept { \
        return utils::string_to_enum<EnumType>(str); \
    } \
    inline constexpr EnumType StringTo##EnumType##OrDefault(std::string_view str, EnumType default_value) noexcept { \
        return utils::string_to_enum_or_default(str, default_value); \
    }

  /**
   * @brief 为指定枚举类型在命名空间内定义转换器
   * @param Namespace 命名空间名称
   * @param EnumType 枚举类型名称
   *
   * 使用示例:
   * DEFINE_ENUM_CONVERTER_IN_NAMESPACE(config, LogLevel)
   * // 在config命名空间内生成LogLevelConverter
   */
#define DEFINE_ENUM_CONVERTER_IN_NAMESPACE(Namespace, EnumType) \
    namespace Namespace { \
        DEFINE_ENUM_CONVERTER(EnumType); \
    }

   /**
    * @brief 在类内部定义枚举转换器的静态方法
    * @param EnumType 枚举类型名称
    *
    * 使用示例:
    * class MyClass {
    *     DEFINE_ENUM_CONVERTER_METHODS(Status)
    * };
    */
#define DEFINE_ENUM_CONVERTER_METHODS(EnumType) \
    static constexpr std::string_view EnumType##ToString(EnumType value) noexcept { \
        return utils::enum_to_string(value); \
    } \
    static constexpr std::optional<EnumType> StringTo##EnumType(const std::string& str) noexcept { \
        return utils::string_to_enum<EnumType>(str); \
    } \
    static constexpr EnumType StringTo##EnumType##OrDefault(std::string_view str, EnumType default_value) noexcept { \
        return utils::string_to_enum_or_default(str, default_value); \
    }

    /**
     * @brief 为枚举类型生成流操作符重载
     * @param EnumType 枚举类型名称
     *
     * 使用示例:
     * DEFINE_ENUM_STREAM_OPERATORS(Status)
     * // 可以使用 std::cout << status_value; 和 std::cin >> status_value;
     */
#define DEFINE_ENUM_STREAM_OPERATORS(EnumType) \
    inline std::ostream& operator<<(std::ostream& os, EnumType value) { \
        auto str = utils::enum_to_string(value); \
        if (str.empty()) { \
            os << "Unknown(" << static_cast<std::underlying_type_t<EnumType>>(value) << ")"; \
        } else { \
            os << str; \
        } \
        return os; \
    } \
    inline std::istream& operator>>(std::istream& is, EnumType& value) { \
        std::string str; \
        is >> str; \
        auto result = utils::string_to_enum<EnumType>(str); \
        if (result.has_value()) { \
            value = result.value(); \
        } else { \
            is.setstate(std::ios::failbit); \
        } \
        return is; \
    }

     /**
      * @brief 生成完整的枚举支持（转换器 + 流操作符 + 便利函数）
      * @param EnumType 枚举类型名称
      *
      * 使用示例:
      * DEFINE_ENUM_SUPPORT(Status)
      * // 生成所有相关的转换器、函数和操作符
      */
#define DEFINE_ENUM_SUPPORT(EnumType) \
    DEFINE_ENUM_CONVERTER_FULL(EnumType); \
    DEFINE_ENUM_STREAM_OPERATORS(EnumType)

      /**
       * @brief 快速定义枚举和其转换支持
       * @param EnumName 枚举名称
       * @param ... 枚举值列表
       *
       * 使用示例:
       * DEFINE_ENUM_WITH_CONVERTER(Status, Pending, InProgress, Completed, Failed)
       */
#define DEFINE_ENUM_WITH_CONVERTER(EnumName, ...) \
    enum class EnumName { __VA_ARGS__ }; \
    DEFINE_ENUM_SUPPORT(EnumName)
       // =========================== 使用示例 ===========================
       // #include "utils/EnumConvert.hpp"
       // #include <iostream>

       //        // 方式1: 先定义枚举，然后使用宏生成转换器
       // enum class Status {
       //     Pending,
       //     InProgress,
       //     Completed,
       //     Failed
       // };

       // // 生成完整的转换器支持
       // DEFINE_ENUM_SUPPORT(Status)

       // // 方式2: 一次性定义枚举和转换器
       // DEFINE_ENUM_WITH_CONVERTER(Priority, Low, Medium, High, Critical)

       // // 方式3: 只生成转换器类型别名
       // enum class LogLevel {
       //     Debug, Info, Warning, Error
       // };
       // DEFINE_ENUM_CONVERTER(LogLevel)

       // // 方式4: 在命名空间中定义
       // namespace config {
       //     enum class Theme { Light, Dark, Auto };
       // }
       // DEFINE_ENUM_CONVERTER_IN_NAMESPACE(config, Theme)

       // // 方式5: 在类中定义转换方法
       // class FileManager {
       //     enum class FileType { Text, Binary, Image, Video };

       // public:
       //     DEFINE_ENUM_CONVERTER_METHODS(FileType)

       //         void processFile(FileType type) {
       //         std::cout << "Processing " << FileTypeConverter::toString(type) << " file\n";
       //     }
       // };

       // int main() {
       //     // 使用生成的转换器
       //     Status status = Status::InProgress;

       //     // 使用流操作符
       //     std::cout << "Current status: " << status << std::endl;

       //     // 使用便利函数
       //     std::cout << "Status string: " << StatusToString(status) << std::endl;

       //     // 字符串转枚举
       //     auto parsed = StringToStatus("Completed");
       //     if (parsed.has_value()) {
       //         std::cout << "Parsed: " << parsed.value() << std::endl;
       //     }

       //     // 使用Priority枚举
       //     Priority p = Priority::High;
       //     std::cout << "Priority: " << p << std::endl;

       //     // 使用LogLevel转换器
       //     LogLevelConverter converter;
       //     std::cout << "Debug level: " << converter.toString(LogLevel::Debug) << std::endl;

       //     // 使用命名空间中的转换器
       //     config::Theme theme = config::Theme::Dark;
       //     std::cout << "Theme: " << config::ThemeConverter::toString(theme) << std::endl;

       //     return 0;
       // }

#endif //ENUM_CONVERT_HPP
