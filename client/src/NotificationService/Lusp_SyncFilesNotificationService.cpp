/**
 * @file Lusp_SyncFilesNotificationService.cpp
 * @brief Lusp_SyncFilesNotificationService类的实现文件。
 *
 * 实现了异步监管上传队列、自动pop并通过socket发送到本地服务的核心逻辑。
 */
#include "NotificationService/Lusp_SyncFilesNotificationService.h"
#include <iostream>
#include <chrono>
#include <future>
#include "UniConv.h"
#include "log_headers.h"
#include "SyncUploadQueue/Lusp_SyncUploadQueue.h"
#include "Lusp_SyncUploadQueuePrivate.h"
#include "AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.h"
#include "Config/ClientConfigManager.h"
#include "asio/asio/io_context.hpp"
#include "upload_file_info_generated.h"
#include "flatbuffers/flatbuffers.h"


 /**
  * @brief 构造函数，内部自动创建io_context和IPC客户端并管理线程，外部必须传队列和配置管理器。析构时自动清理。
  * @param queue 需要被监管的上传队列引用。
  * @param configMgr 客户端配置管理器引用（必需）。
  */
Lusp_SyncFilesNotificationService::Lusp_SyncFilesNotificationService(Lusp_SyncUploadQueue& queue, const ClientConfigManager& configMgr)
    : queueRef(queue), shouldStop(false), processedCount(0), totalLatencyUs_(0), configMgr_(&configMgr) {

    // 自动管理io_context和IPC客户端
    ioContext_ = std::make_shared<asio::io_context>();
    ipcClient_ = std::make_shared<Lusp_AsioLoopbackIpcClient>(*ioContext_, configMgr);
    ipcClient_->connect();

    // 自动设置socketSendFunc为FlatBuffers序列化并发送
    setSocketSendFunc([this](const Lusp_SyncUploadFileInfo& info) {
        std::string msg = ToFlatBuffer(info);
        if (ipcClient_) {
            ipcClient_->send(msg);
        }
        });

    // 启动io_context线程
    ioThread_ = std::thread([this]() { ioContext_->run(); });
}

/**
 * @brief 析构函数，自动停止监管线程。
 */
Lusp_SyncFilesNotificationService::~Lusp_SyncFilesNotificationService() {
    try {
        //  停止通知线程（带超时保护）
        stop();

        //  关闭 IPC 客户端（取消所有待处理的异步操作）
        if (ipcClient_) {
            ipcClient_->disconnect();
        }

        //  停止 io_context （先停止再 join）
        if (ioContext_) {
            ioContext_->stop();
        }

        //  等待 ioThread 退出（带超时）
        if (ioThread_.joinable()) {
            // 等待最多 3 秒
            auto future = std::async(std::launch::async, [this]() {
                if (ioThread_.joinable()) {
                    ioThread_.join();
                }
                });

            if (future.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
                g_LogSyncNotificationService.WriteLogContent(LOG_WARN,
                    "ioThread did not exit within 3 seconds, detaching...");
                ioThread_.detach();  // 超时后分离线程
            }
        }

        g_LogSyncNotificationService.WriteLogContent(LOG_INFO,
            "NotificationService destroyed successfully");

    }
    catch (const std::exception& e) {
        g_LogSyncNotificationService.WriteLogContent(LOG_ERROR,
            "Exception in destructor: " + std::string(e.what()));
    }
    catch (...) {
        g_LogSyncNotificationService.WriteLogContent(LOG_ERROR,
            "Unknown exception in destructor");
    }
}

/**
 * @brief 启动异步监管线程。
 */
void Lusp_SyncFilesNotificationService::start() {
    shouldStop = false;
    notifyThread = std::thread([this]() { notificationLoop(); });
}

/**
 * @brief 停止监管线程，安全退出。
 */
void Lusp_SyncFilesNotificationService::stop() {
    if (!shouldStop.exchange(true)) {  // 防止重复调用
        g_LogSyncNotificationService.WriteLogContent(LOG_INFO,
            "Stopping NotificationService...");

        // 直接通知条件变量（更可靠）
        if (queueRef.d) {
            queueRef.d->uploadQueue.notifyAll();  // 唤醒所有等待的线程
        }

        // 等待线程退出（带超时）
        if (notifyThread.joinable()) {
            auto future = std::async(std::launch::async, [this]() {
                if (notifyThread.joinable()) {
                    notifyThread.join();
                }
                });

            if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
                g_LogSyncNotificationService.WriteLogContent(LOG_WARN,
                    "notifyThread did not exit within 5 seconds, detaching...");
                notifyThread.detach();  // 超时后分离线程
            }
            else {
                g_LogSyncNotificationService.WriteLogContent(LOG_INFO,
                    "notifyThread stopped successfully");
            }
        }
    }
}

/**
 * @brief 设置socket通信回调。
 * @param func 发送到本地服务的回调函数。
 */
void Lusp_SyncFilesNotificationService::setSocketSendFunc(SocketSendFunc func) {
    socketSendFunc = func;
}

/**
 * @brief 获取已处理任务数。
 * @return 已处理的文件数。
 */
size_t Lusp_SyncFilesNotificationService::getProcessedCount() const {
    return processedCount.load();
}

/**
 * @brief 获取平均处理延迟（ms）。
 * @return 平均延迟，单位毫秒。
 */
double Lusp_SyncFilesNotificationService::getAverageLatencyMs() const {
    size_t count = processedCount.load(std::memory_order_relaxed);
    uint64_t totalUs = totalLatencyUs_.load(std::memory_order_relaxed);
    return count ? (static_cast<double>(totalUs) / count / 1000.0) : 0.0;  // 微秒转毫秒
}

/**
 * @brief 导出当前服务状态诊断信息。
 * @return 状态字符串。
 */
std::string Lusp_SyncFilesNotificationService::dumpStatus() const {
    size_t processed = processedCount.load(std::memory_order_relaxed);
    size_t errors = errorCount_.load(std::memory_order_relaxed);
    double errorRate = processed > 0 ? (static_cast<double>(errors) / processed * 100.0) : 0.0;

    return "[NotificationService] Processed: " + std::to_string(processed) +
        ", Errors: " + std::to_string(errors) +
        " (" + std::to_string(errorRate) + "%)" +
        ", AvgLatency(ms): " + std::to_string(getAverageLatencyMs());
}

/**
 * @brief 监管线程主循环，阻塞等待队列有内容时自动pop并通过socket发送到本地服务。
 *        支持处理延迟统计、异常处理和错误恢复。
 */
void Lusp_SyncFilesNotificationService::notificationLoop() {
    g_LogSyncNotificationService.WriteLogContent(LOG_DEBUG,
        "notificationLoop started");

    while (!shouldStop.load(std::memory_order_relaxed)) {
        try {
            Lusp_SyncUploadFileInfo fileInfo;

            //  waitAndPop 被 notify_all() 唤醒后，检查返回值
            if (queueRef.d && queueRef.d->uploadQueue.waitAndPop(fileInfo)) {
                // 检查是否需要停止（被 notify_all 唤醒）
                if (shouldStop.load(std::memory_order_relaxed)) {
                    g_LogSyncNotificationService.WriteLogContent(LOG_INFO,
                        "notificationLoop received stop signal (after waitAndPop), exiting...");
                    break;
                }

                // 方案A兼容：遇到空文件名（哨兵对象）也退出
                if (fileInfo.sFileFullNameValue.empty()) {
                    g_LogSyncNotificationService.WriteLogContent(LOG_DEBUG,
                        "Received sentinel object, exiting...");
                    break;
                }

                //  无锁延迟统计：使用原子操作累加微秒级延迟
                auto now = std::chrono::steady_clock::now();
                auto latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(now - fileInfo.enqueueTime).count();
                totalLatencyUs_.fetch_add(latencyUs, std::memory_order_relaxed);  // 原子累加，无需加锁
                processedCount.fetch_add(1, std::memory_order_relaxed);

                // 通过socket发送到本地服务（带异常捕获）
                try {
                    if (socketSendFunc) {
                        socketSendFunc(fileInfo);
                    }

                    // 发送成功日志（仅记录到日志文件，不输出控制台）
                    g_LogSyncNotificationService.WriteLogContent(
                        LOG_INFO,
                        "通过socket发送: " + UniConv::GetInstance()->ToUtf8FromUtf16LE(fileInfo.sFileFullNameValue)
                    );

                }
                catch (const std::exception& sendEx) {
                    //  Socket 发送异常（不中断主循环）
                    errorCount_.fetch_add(1, std::memory_order_relaxed);
                    g_LogSyncNotificationService.WriteLogContent(
                        LOG_ERROR,
                        "Socket发送失败: " + UniConv::GetInstance()->ToUtf8FromUtf16LE(fileInfo.sFileFullNameValue) +
                        ", 错误: " + std::string(sendEx.what())
                    );
                }
                catch (...) {
                    //  未知异常
                    errorCount_.fetch_add(1, std::memory_order_relaxed);
                    g_LogSyncNotificationService.WriteLogContent(
                        LOG_ERROR,
                        "Socket发送失败(未知异常): " + UniConv::GetInstance()->ToUtf8FromUtf16LE(fileInfo.sFileFullNameValue)
                    );
                }
            }
            else {
                // waitAndPop 返回 false（队列停止信号）
                if (shouldStop.load(std::memory_order_relaxed)) {
                    g_LogSyncNotificationService.WriteLogContent(LOG_INFO,
                        "waitAndPop returned false, shouldStop=true, exiting...");
                    break;
                }
                // 意外情况：waitAndPop 返回 false 但 shouldStop 未设置
                g_LogSyncNotificationService.WriteLogContent(LOG_WARN,
                    "waitAndPop returned false but shouldStop=false, continuing...");
            }

        }
        catch (const std::exception& e) {
            //  队列操作或其他异常（不中断主循环）
            errorCount_.fetch_add(1, std::memory_order_relaxed);
            g_LogSyncNotificationService.WriteLogContent(
                LOG_ERROR,
                "NotificationService异常: " + std::string(e.what())
            );
            // 短暂休眠避免异常
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        }
        catch (...) {
            //  未知异常（不中断主循环）
            errorCount_.fetch_add(1, std::memory_order_relaxed);
            g_LogSyncNotificationService.WriteLogContent(
                LOG_ERROR,
                "NotificationService未知异常"
            );
            // 短暂休眠避免异常
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // 线程退出时记录统计信息
    g_LogSyncNotificationService.WriteLogContent(
        LOG_INFO,
        "NotificationService退出 - 已处理: " + std::to_string(processedCount.load()) +
        ", 错误: " + std::to_string(errorCount_.load()) +
        ", 平均延迟: " + std::to_string(getAverageLatencyMs()) + "ms"
    );
}

void Lusp_SyncFilesNotificationService::setIpcClient(std::shared_ptr<Lusp_AsioLoopbackIpcClient> ipcClient) {
    ipcClient_ = ipcClient;
    // 设置socketSendFunc为自动序列化并发送FlatBuffers
    setSocketSendFunc([this](const Lusp_SyncUploadFileInfo& info) {
        std::string out = ToFlatBuffer(info);
        if (ipcClient_) {
            ipcClient_->send(out);
        }
        });
}



std::string Lusp_SyncFilesNotificationService::ToFlatBuffer(const Lusp_SyncUploadFileInfo& info) {
    flatbuffers::FlatBufferBuilder builder;
    using namespace UploadClient::Sync;

    auto s_lan_client_device = builder.CreateString(UniConv::GetInstance()->ToUtf8FromUtf16LE(info.sLanClientDevice));
    auto s_file_full_name_value = builder.CreateString(UniConv::GetInstance()->ToUtf8FromUtf16LE(info.sFileFullNameValue));
    auto s_only_file_name_value = builder.CreateString(UniConv::GetInstance()->ToUtf8FromUtf16LE(info.sOnlyFileNameValue));
    auto s_file_record_time_value = builder.CreateString(info.sFileRecordTimeValue);
    auto s_file_md5_value_info = builder.CreateString(info.sFileMd5ValueInfo);
    auto s_auth_token_values = builder.CreateString(info.sAuthTokenValues);
    auto s_description_info = builder.CreateString(UniConv::GetInstance()->ToUtf8FromUtf16LE(info.sDescriptionInfo));

    auto fb = CreateFBS_SyncUploadFileInfo(
        builder,
        static_cast<FBS_SyncUploadFileTyped>(static_cast<int>(info.eUploadFileTyped)),
        s_lan_client_device,
        static_cast<uint64_t>(info.sSyncFileSizeValue),
        s_file_full_name_value,
        s_only_file_name_value,
        s_file_record_time_value,
        s_file_md5_value_info,
        static_cast<FBS_SyncFileExistPolicy>(static_cast<int>(info.eFileExistPolicy)),
        s_auth_token_values,
        info.uUploadTimeStamp,
        static_cast<FBS_SyncUploadStatusInf>(static_cast<int>(info.eUploadStatusInf)),
        s_description_info,
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(info.enqueueTime.time_since_epoch()).count())
    );
    builder.Finish(fb);
    return std::string(reinterpret_cast<const char*>(builder.GetBufferPointer()), builder.GetSize());
}

Lusp_SyncUploadFileInfo Lusp_SyncFilesNotificationService::FromFlatBuffer(const uint8_t* buf, size_t size) {
    using namespace UploadClient::Sync;
    Lusp_SyncUploadFileInfo info{};
    // 反序列化为 FlatBuffers 对象
    auto fb = GetFBS_SyncUploadFileInfo(buf);
    if (!fb) return info;
    // 字段映射
    info.eUploadFileTyped = static_cast<Lusp_UploadFileTyped>(static_cast<int>(fb->e_upload_file_typed()));
    info.sLanClientDevice = UniConv::GetInstance()->ToUtf16LEFromLocale(fb->s_lan_client_device() ? fb->s_lan_client_device()->str() : "");
    info.sSyncFileSizeValue = static_cast<size_t>(fb->s_sync_file_size_value());
    info.sFileFullNameValue = UniConv::GetInstance()->ToUtf16LEFromLocale(fb->s_file_full_name_value() ? fb->s_file_full_name_value()->str() : "");
    info.sOnlyFileNameValue = UniConv::GetInstance()->ToUtf16LEFromLocale(fb->s_only_file_name_value() ? fb->s_only_file_name_value()->str() : "");
    info.sFileRecordTimeValue = fb->s_file_record_time_value() ? fb->s_file_record_time_value()->str() : "";
    info.sFileMd5ValueInfo = fb->s_file_md5_value_info() ? fb->s_file_md5_value_info()->str() : "";
    info.eFileExistPolicy = static_cast<Lusp_FileExistPolicy>(static_cast<int>(fb->e_file_exist_policy()));
    info.sAuthTokenValues = fb->s_auth_token_values() ? fb->s_auth_token_values()->str() : "";
    info.uUploadTimeStamp = fb->u_upload_time_stamp();
    info.eUploadStatusInf = static_cast<Lusp_UploadStatusInf>(static_cast<int>(fb->e_upload_status_inf()));
    info.sDescriptionInfo = UniConv::GetInstance()->ToUtf16LEFromLocale(fb->s_description_info() ? fb->s_description_info()->str() : "");
    // FlatBuffers 里 enqueue_time_ms 是 uint64_t 毫秒
    info.enqueueTime = std::chrono::steady_clock::time_point(std::chrono::milliseconds(fb->enqueue_time_ms()));
    return info;
}
