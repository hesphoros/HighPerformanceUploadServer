#include "log_headers.h"
#include <UniConv.h>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <chrono>
#include <iomanip>
#include <thread>

// 全局LoggerWrapper实例
LoggerWrapper g_luspLogWriteImpl;
LoggerWrapper g_LogSyncUploadQueueInfo;
LoggerWrapper g_LogSyncNotificationService;
LoggerWrapper g_LogAsioLoopbackIpcClient;

// 获取当前日期时间字符串（用于文件名）
std::string getCurrentDateTimeString() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_info;
#ifdef _WIN32
    localtime_s(&tm_info, &time);
#else
    localtime_r(&time, &tm_info);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_info, "%Y_%m_%d")
        << (tm_info.tm_hour >= 12 ? "_PM" : "_AM");
    return oss.str();
}

void initializeLogging() {
    // 注意：不设置 SetDefaultEncoding，让 UniConv 使用系统默认编码 (CP936/GBK)
    // spdlog 日志文件仍然是 UTF-8 编码，但 ToLocaleFrom* 方法会返回 GBK

    // 确保日志目录存在
    std::filesystem::path logPath = std::filesystem::current_path() / "log";
    if (!std::filesystem::exists(logPath)) {
        std::filesystem::create_directories(logPath);
    }

    try {
        // 初始化 spdlog 异步日志
        // 设置线程池大小：8192个队列大小，1个后台线程
        spdlog::init_thread_pool(8192, 1);

        // 获取当前日期时间字符串
        std::string datetime = getCurrentDateTimeString();

        // 创建异步rotating file sink
        // 参数：文件名，最大文件大小(10MB)，最多保留文件数(3)
        auto uploadclient_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            (logPath / ("UploadClient-" + datetime + ".log")).string(),
            1024 * 1024 * 10, 3);

        auto syncqueue_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            (logPath / ("SyncUploadQueue-" + datetime + ".log")).string(),
            1024 * 1024 * 10, 3);

        auto notification_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            (logPath / ("SyncNotification-" + datetime + ".log")).string(),
            1024 * 1024 * 10, 3);

        auto ipc_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            (logPath / ("AsioLoopbackIpcClient-" + datetime + ".log")).string(),
            1024 * 1024 * 10, 3);

        // 创建异步logger
        auto uploadclient_logger = std::make_shared<spdlog::async_logger>(
            "UploadClient",
            uploadclient_sink,
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);

        auto syncqueue_logger = std::make_shared<spdlog::async_logger>(
            "SyncUploadQueue",
            syncqueue_sink,
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);

        auto notification_logger = std::make_shared<spdlog::async_logger>(
            "SyncNotification",
            notification_sink,
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);

        auto ipc_logger = std::make_shared<spdlog::async_logger>(
            "AsioLoopbackIpcClient",
            ipc_sink,
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);

        // 注册所有logger
        spdlog::register_logger(uploadclient_logger);
        spdlog::register_logger(syncqueue_logger);
        spdlog::register_logger(notification_logger);
        spdlog::register_logger(ipc_logger);

        // 设置日志格式：[时间] [日志级别] 消息
        uploadclient_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        syncqueue_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        notification_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        ipc_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

        // 设置日志级别
        spdlog::set_level(spdlog::level::debug);

        // 设置自动flush
        spdlog::flush_every(std::chrono::seconds(3));

        // 将spdlog logger包装到LoggerWrapper中
        g_luspLogWriteImpl.setLogger(uploadclient_logger);
        g_LogSyncUploadQueueInfo.setLogger(syncqueue_logger);
        g_LogSyncNotificationService.setLogger(notification_logger);
        g_LogAsioLoopbackIpcClient.setLogger(ipc_logger);

        uploadclient_logger->info("日志系统初始化完成 (使用spdlog异步日志)");


    }
    catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "日志初始化失败: " << ex.what() << std::endl;
    }
}

void shutdownLogging() {
    try {
        // 先flush所有日志，确保之前的日志都写入文件
        spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) {
            logger->flush();
            });

        // 记录关闭日志
        g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "日志系统正在关闭...");

        // 再次flush确保关闭消息也写入
        spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) {
            logger->flush();
            });

        // 等待一小段时间确保异步日志完全写入
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 关闭所有logger
        spdlog::shutdown();
    }
    catch (const std::exception& ex) {
        std::cerr << "日志关闭失败: " << ex.what() << std::endl;
    }
}