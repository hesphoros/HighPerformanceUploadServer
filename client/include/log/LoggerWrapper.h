#ifndef INCLUDE_LOGGER_WRAPPER_H
#define INCLUDE_LOGGER_WRAPPER_H

#include <spdlog/spdlog.h>
#include <memory>
#include <string>
#include "UniConv.h"

/**
 * @brief 日志适配器类 - 用于包装spdlog并提供与旧日志系统兼容的API
 * @details 负责将日志消息写入到 spdlog，支持多种字符串编码
 * @note 职责：仅处理日志写入，不包含业务逻辑（如错误码转换）
 */
class LoggerWrapper {
public:
    explicit LoggerWrapper(std::shared_ptr<spdlog::logger> logger) : logger_(logger) {}

    LoggerWrapper() = default;

    void setLogger(std::shared_ptr<spdlog::logger> logger) {
        logger_ = logger;
    }

    void WriteLogContent(const std::string& level, const std::string& message) {
        if (!logger_) return;

        if (level.find("INFO") != std::string::npos || level.find("OK") != std::string::npos) {
            logger_->info(message);
        }
        else if (level.find("ERROR") != std::string::npos) {
            logger_->error(message);
        }
        else if (level.find("DEBUG") != std::string::npos) {
            logger_->debug(message);
        }
        else if (level.find("WARN") != std::string::npos) {
            logger_->warn(message);
        }
        else if (level.find("FATAL") != std::string::npos) {
            logger_->critical(message);
        }
        else if (level.find("TRACE") != std::string::npos) {
            logger_->trace(message);
        }
        else {
            logger_->info(message);
        }
    }

    void WriteLogContent(const std::wstring& level, const std::wstring& message) {
        // Windows: wstring 是 UTF-16LE
        // 需要转换为 UTF-8 以供 spdlog 写入
        std::u16string level_u16(level.begin(), level.end());
        std::u16string message_u16(message.begin(), message.end());
        WriteLogContent(
            UniConv::GetInstance()->ToUtf8FromUtf16LE(level_u16),
            UniConv::GetInstance()->ToUtf8FromUtf16LE(message_u16)
        );
    }

    void WriteLogContent(const std::u16string& level, const std::u16string& message) {
        // UTF-16LE 转换为 UTF-8
        WriteLogContent(
            UniConv::GetInstance()->ToUtf8FromUtf16LE(level),
            UniConv::GetInstance()->ToUtf8FromUtf16LE(message)
        );
    }

private:
    std::shared_ptr<spdlog::logger> logger_;
};

#endif // INCLUDE_LOGGER_WRAPPER_H
