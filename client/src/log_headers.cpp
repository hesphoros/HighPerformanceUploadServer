#include "log_headers.h"
#include <UniConv.h>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <chrono>
#include <iomanip>
#include <thread>
#include <csignal>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

// 全局LoggerWrapper实例
LoggerWrapper g_luspLogWriteImpl;
LoggerWrapper g_LogSyncUploadQueueInfo;
LoggerWrapper g_LogSyncNotificationService;
LoggerWrapper g_LogAsioLoopbackIpcClient;
LoggerWrapper g_LogMessageQueue;  // 新增：消息队列日志

// 标记是否正在关闭日志系统（避免重复调用）
static std::atomic<bool> g_isShuttingDown{ false };

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

// 强制刷新所有日志（用于信号处理和异常退出）
void forceFlushAllLogs() {
    try {
        spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) {
            if (logger) {
                logger->flush();
            }
            });
    }
    catch (...) {
        // 忽略异常，确保不会在信号处理中崩溃
    }
}

// 信号处理函数
void signalHandler(int signal) {
    if (g_isShuttingDown.exchange(true)) {
        // 已经在关闭中，直接退出
        std::_Exit(1);
    }

    // 记录信号信息
    const char* signal_name = "UNKNOWN";
    switch (signal) {
    case SIGINT:  signal_name = "SIGINT (Ctrl+C)"; break;
    case SIGTERM: signal_name = "SIGTERM"; break;
    case SIGABRT: signal_name = "SIGABRT"; break;
#ifdef _WIN32
    case SIGBREAK: signal_name = "SIGBREAK (Ctrl+Break)"; break;
#endif
    }

    // 写入紧急日志
    g_luspLogWriteImpl.WriteLogContent(LOG_WARN,
        std::string("捕获到信号: ") + signal_name + ", 正在安全关闭...");

    // 强制刷新所有日志
    forceFlushAllLogs();

    // 等待日志写入完成（更长的等待时间）
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 再次刷新确保万无一失
    forceFlushAllLogs();

    // 调用正常关闭流程
    shutdownLogging();

    // 退出程序
    std::_Exit(signal);
}

#ifdef _WIN32
// Windows 控制台事件处理器
BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT ||
        signal == CTRL_BREAK_EVENT || signal == CTRL_LOGOFF_EVENT ||
        signal == CTRL_SHUTDOWN_EVENT) {

        if (g_isShuttingDown.exchange(true)) {
            return TRUE;
        }

        const char* event_name = "UNKNOWN";
        switch (signal) {
        case CTRL_C_EVENT:      event_name = "CTRL_C_EVENT"; break;
        case CTRL_CLOSE_EVENT:  event_name = "CTRL_CLOSE_EVENT"; break;
        case CTRL_BREAK_EVENT:  event_name = "CTRL_BREAK_EVENT"; break;
        case CTRL_LOGOFF_EVENT: event_name = "CTRL_LOGOFF_EVENT"; break;
        case CTRL_SHUTDOWN_EVENT: event_name = "CTRL_SHUTDOWN_EVENT"; break;
        }

        g_luspLogWriteImpl.WriteLogContent(LOG_WARN,
            std::string("捕获到Windows控制台事件: ") + event_name + ", 正在安全关闭...");

        forceFlushAllLogs();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        forceFlushAllLogs();
        shutdownLogging();

        return TRUE;
    }
    return FALSE;
}
#endif

// 设置信号处理器
void setupSignalHandlers() {
    // 注册标准信号处理器
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // 终止信号
    std::signal(SIGABRT, signalHandler);  // 异常终止

#ifdef _WIN32
    std::signal(SIGBREAK, signalHandler); // Ctrl+Break
    // 注册 Windows 控制台事件处理器
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#endif

    // 注册 atexit 处理器（程序正常退出时调用）
    std::atexit([]() {
        if (!g_isShuttingDown.exchange(true)) {
            forceFlushAllLogs();
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            shutdownLogging();
        }
        });
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

        auto msgqueue_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            (logPath / ("MessageQueue-" + datetime + ".log")).string(),
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

        auto msgqueue_logger = std::make_shared<spdlog::async_logger>(
            "MessageQueue",
            msgqueue_sink,
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);

        // 注册所有logger
        spdlog::register_logger(uploadclient_logger);
        spdlog::register_logger(syncqueue_logger);
        spdlog::register_logger(notification_logger);
        spdlog::register_logger(ipc_logger);
        spdlog::register_logger(msgqueue_logger);

        // 设置日志格式：[时间] [日志级别] 消息
        uploadclient_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        syncqueue_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        notification_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        ipc_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        msgqueue_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

        // 设置日志级别
        spdlog::set_level(spdlog::level::debug);

        // 设置自动flush - 缩短为1秒，确保日志及时写入
        spdlog::flush_every(std::chrono::seconds(1));

        // 将spdlog logger包装到LoggerWrapper中
        g_luspLogWriteImpl.setLogger(uploadclient_logger);
        g_LogSyncUploadQueueInfo.setLogger(syncqueue_logger);
        g_LogSyncNotificationService.setLogger(notification_logger);
        g_LogAsioLoopbackIpcClient.setLogger(ipc_logger);
        g_LogMessageQueue.setLogger(msgqueue_logger);

        uploadclient_logger->info("日志系统初始化完成 (使用spdlog异步日志, 1秒自动flush)");

        // 设置信号处理器，确保异常退出时日志能够写入
        setupSignalHandlers();
        uploadclient_logger->info("信号处理器已设置，程序异常退出时将自动保存日志");

    }
    catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "日志初始化失败: " << ex.what() << std::endl;
    }
}

void shutdownLogging() {
    try {
        // 先记录关闭日志（在所有 logger 上）
        g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "日志系统正在关闭...");
        g_LogSyncUploadQueueInfo.WriteLogContent(LOG_INFO, "日志系统正在关闭...");
        g_LogSyncNotificationService.WriteLogContent(LOG_INFO, "日志系统正在关闭...");
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO, "日志系统正在关闭...");
        g_LogMessageQueue.WriteLogContent(LOG_INFO, "日志系统正在关闭...");

        // 防止重复关闭（在记录日志之后检查）
        if (g_isShuttingDown.exchange(true)) {
            // 如果已经在关闭流程中，只记录日志但不执行 shutdown
            return;
        }

        // 第一次刷新
        spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) {
            if (logger) {
                logger->flush();
            }
            });

        // 等待异步队列清空（增加到500ms）
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 第二次刷新，确保万无一失
        spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) {
            if (logger) {
                logger->flush();
            }
            });

        // 再等待一小段时间
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 关闭所有logger
        spdlog::shutdown();
    }
    catch (const std::exception& ex) {
        std::cerr << "日志关闭失败: " << ex.what() << std::endl;
    }
    catch (...) {
        std::cerr << "日志关闭时发生未知错误" << std::endl;
    }
}