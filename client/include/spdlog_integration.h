/**
 * @file spdlog_integration.h
 * @brief spdlog日志库集成示例
 * @author HighPerformanceUploadClient Team
 * @date 2025-10-01
 * 
 * 展示如何在项目中集成和使用spdlog库
 * spdlog是一个快速的、仅头文件的C++日志库
 */

#ifndef SPDLOG_INTEGRATION_H
#define SPDLOG_INTEGRATION_H

#include <spdlog/spdlog.h>
#include <spdlog/sinks/file_sinks.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>

namespace UploadClient {

/**
 * @brief spdlog日志管理器
 * 
 * 提供统一的日志配置和管理接口
 */
class SpdlogManager {
public:
    /**
     * @brief 初始化spdlog日志系统
     * @param logDir 日志目录
     * @param logLevel 日志级别 (trace, debug, info, warn, error, critical)
     * @return 是否初始化成功
     */
    static bool Initialize(const std::string& logDir = "logs", const std::string& logLevel = "info") {
        try {
            // 创建控制台输出sink (带颜色)
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info);
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");

            // 创建旋转文件sink (10MB文件，保留5个)
            auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                logDir + "/upload_client.log", 1024 * 1024 * 10, 5);
            rotating_sink->set_level(spdlog::level::trace);
            rotating_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%s:%#] %v");

            // 创建多输出日志器
            std::vector<spdlog::sink_ptr> sinks {console_sink, rotating_sink};
            auto logger = std::make_shared<spdlog::logger>("upload_client", sinks.begin(), sinks.end());
            
            // 设置日志级别
            if (logLevel == "trace") logger->set_level(spdlog::level::trace);
            else if (logLevel == "debug") logger->set_level(spdlog::level::debug);
            else if (logLevel == "info") logger->set_level(spdlog::level::info);
            else if (logLevel == "warn") logger->set_level(spdlog::level::warn);
            else if (logLevel == "error") logger->set_level(spdlog::level::err);
            else if (logLevel == "critical") logger->set_level(spdlog::level::critical);
            else logger->set_level(spdlog::level::info);

            // 注册为默认日志器
            spdlog::register_logger(logger);
            spdlog::set_default_logger(logger);
            
            // 设置刷新策略
            spdlog::flush_every(std::chrono::seconds(3));
            spdlog::flush_on(spdlog::level::err);

            SPDLOG_INFO("spdlog日志系统初始化成功");
            SPDLOG_INFO("日志目录: {}", logDir);
            SPDLOG_INFO("日志级别: {}", logLevel);
            
            return true;
        }
        catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "spdlog初始化失败: " << ex.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief 创建专用日志器
     * @param name 日志器名称
     * @param filename 日志文件名
     * @return 日志器指针
     */
    static std::shared_ptr<spdlog::logger> CreateLogger(const std::string& name, 
                                                       const std::string& filename) {
        try {
            auto logger = spdlog::rotating_logger_mt(name, filename, 1024 * 1024 * 5, 3);
            logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
            logger->set_level(spdlog::level::info);
            return logger;
        }
        catch (const spdlog::spdlog_ex& ex) {
            SPDLOG_ERROR("创建日志器失败 {}: {}", name, ex.what());
            return nullptr;
        }
    }

    /**
     * @brief 关闭日志系统
     */
    static void Shutdown() {
        spdlog::shutdown();
    }
};

/**
 * @brief 便捷的日志宏定义
 */
#define LOG_TRACE(...)    SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

/**
 * @brief 性能计时器 (结合spdlog)
 */
class PerformanceTimer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::string operation_name;

public:
    explicit PerformanceTimer(const std::string& name) 
        : start_time(std::chrono::high_resolution_clock::now())
        , operation_name(name) {
        LOG_DEBUG("开始执行: {}", operation_name);
    }

    ~PerformanceTimer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        LOG_INFO("操作 '{}' 完成，耗时: {}ms", operation_name, duration.count());
    }
};

/**
 * @brief 便捷的性能计时宏
 */
#define PERF_TIMER(name) PerformanceTimer _timer(name)

} // namespace UploadClient

/**
 * @brief 使用示例
 * 
 * ```cpp
 * #include "spdlog_integration.h"
 * 
 * int main() {
 *     // 初始化日志系统
 *     UploadClient::SpdlogManager::Initialize("logs", "debug");
 *     
 *     // 使用便捷宏
 *     LOG_INFO("应用程序启动");
 *     LOG_DEBUG("调试信息: 连接到服务器 {}", "localhost:8080");
 *     LOG_WARN("警告: 网络延迟较高");
 *     LOG_ERROR("错误: 文件上传失败 - {}", error_msg);
 *     
 *     // 性能计时
 *     {
 *         PERF_TIMER("文件上传操作");
 *         // ... 执行上传操作
 *     }
 *     
 *     // 创建专用日志器
 *     auto upload_logger = UploadClient::SpdlogManager::CreateLogger(
 *         "upload", "logs/upload.log");
 *     upload_logger->info("上传进度: {}%", progress);
 *     
 *     // 关闭日志系统
 *     UploadClient::SpdlogManager::Shutdown();
 *     return 0;
 * }
 * ```
 */

#endif // SPDLOG_INTEGRATION_H