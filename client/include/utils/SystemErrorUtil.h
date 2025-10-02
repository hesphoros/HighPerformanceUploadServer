#ifndef INCLUDE_UTILS_SYSTEM_ERROR_UTIL_H
#define INCLUDE_UTILS_SYSTEM_ERROR_UTIL_H

#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief 系统错误工具类 - 提供错误码转换和格式化功能
 * @details 将 std::error_code 转换为可读的 UTF-8 字符串
 */
class SystemErrorUtil {
public:
    /**
     * @brief 将 error_code 转换为 UTF-8 格式的错误消息
     * @param ec error_code 对象
     * @param includeCode 是否在消息末尾包含错误码 (默认: true)
     * @return UTF-8 编码的错误消息字符串
     *
     * @example
     *   std::error_code ec = ...;
     *   std::string msg = SystemErrorUtil::GetErrorMessage(ec);
     *   // 输出: "由于目标计算机积极拒绝，无法连接。 (code: 10061)"
     */
    static std::string GetErrorMessage(const std::error_code& ec, bool includeCode = true) {
        // 如果没有错误，返回成功信息
        if (!ec) {
            return "Success";
        }

        int error_code = ec.value();

#ifdef _WIN32
        // Windows: 使用 FormatMessageW 直接获取 Unicode 错误消息
        wchar_t* messageBuffer = nullptr;
        DWORD size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error_code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&messageBuffer,
            0,
            NULL
        );

        if (size > 0 && messageBuffer != nullptr) {
            // 移除末尾的换行符
            std::wstring wideMsg(messageBuffer);
            LocalFree(messageBuffer);

            while (!wideMsg.empty() && (wideMsg.back() == L'\r' || wideMsg.back() == L'\n')) {
                wideMsg.pop_back();
            }

            // 使用 WideCharToMultiByte 直接转换 UTF-16 → UTF-8
            try {
                int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wideMsg.c_str(), -1, NULL, 0, NULL, NULL);
                if (utf8Size > 0) {
                    std::string utf8Msg(utf8Size - 1, '\0');  // -1 to exclude null terminator
                    WideCharToMultiByte(CP_UTF8, 0, wideMsg.c_str(), -1, &utf8Msg[0], utf8Size, NULL, NULL);

                    if (includeCode) {
                        return utf8Msg + " (code: " + std::to_string(error_code) + ")";
                    }
                    return utf8Msg;
                }
            }
            catch (...) {
                // 转换失败，继续使用降级策略
            }
        }

        // 降级策略：使用错误码
        return "Error code: " + std::to_string(error_code) +
            " (category: " + std::string(ec.category().name()) + ")";
#else
        // Linux/Mac: 使用标准 message()
        std::string msg = ec.message();
        if (msg.empty()) {
            return "Error code: " + std::to_string(error_code) +
                " (category: " + std::string(ec.category().name()) + ")";
        }

        if (includeCode) {
            return msg + " (code: " + std::to_string(error_code) + ")";
        }
        return msg;
#endif
    }

    /**
     * @brief 仅获取错误码数值
     * @param ec error_code 对象
     * @return 错误码数值
     */
    static int GetErrorCode(const std::error_code& ec) {
        return ec.value();
    }

    /**
     * @brief 获取错误类别名称
     * @param ec error_code 对象
     * @return 错误类别名称 (如 "system", "generic" 等)
     */
    static std::string GetErrorCategory(const std::error_code& ec) {
        return ec.category().name();
    }

    /**
     * @brief 格式化错误消息 (前缀 + 错误消息)
     * @param prefix 前缀字符串 (如 "[IPC] 连接失败: ")
     * @param ec error_code 对象
     * @param includeCode 是否包含错误码
     * @return 格式化后的完整消息
     */
    static std::string FormatError(const std::string& prefix, const std::error_code& ec, bool includeCode = true) {
        return prefix + GetErrorMessage(ec, includeCode);
    }
};

#endif // INCLUDE_UTILS_SYSTEM_ERROR_UTIL_H
