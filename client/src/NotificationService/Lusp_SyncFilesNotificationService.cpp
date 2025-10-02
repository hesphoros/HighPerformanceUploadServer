/**
 * @file Lusp_SyncFilesNotificationService.cpp
 * @brief Lusp_SyncFilesNotificationService类的实现文件。
 *
 * 实现了异步监管上传队列、自动pop并通过socket发送到本地服务的核心逻辑。
 */
#include "NotificationService/Lusp_SyncFilesNotificationService.h"
#include <iostream>
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
    : queueRef(queue), shouldStop(false), totalLatencyMs(0), processedCount(0), configMgr_(&configMgr) {

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
    stop();
    if (ioContext_) ioContext_->stop();
    if (ioThread_.joinable()) ioThread_.join();
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
    shouldStop = true;
    // 方案A：push 哨兵对象唤醒线程
    if (queueRef.d) {
        Lusp_SyncUploadFileInfo dummy;
        queueRef.d->uploadQueue.push(dummy);
    }
    if (notifyThread.joinable()) {
        notifyThread.join();
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
    size_t count = processedCount.load();
    std::unique_lock<std::mutex> lock(latencyMutex);
    return count ? (totalLatencyMs / count) : 0.0;
}

/**
 * @brief 导出当前服务状态诊断信息。
 * @return 状态字符串。
 */
std::string Lusp_SyncFilesNotificationService::dumpStatus() const {
    return "[NotificationService] Processed: " + std::to_string(getProcessedCount()) +
        ", AvgLatency(ms): " + std::to_string(getAverageLatencyMs());
}

/**
 * @brief 监管线程主循环，阻塞等待队列有内容时自动pop并通过socket发送到本地服务。
 *        支持处理延迟统计和回调。
 */
void Lusp_SyncFilesNotificationService::notificationLoop() {
    while (!shouldStop) {
        Lusp_SyncUploadFileInfo fileInfo;
        if (queueRef.d && queueRef.d->uploadQueue.waitAndPop(fileInfo)) {
            // 方案A：遇到哨兵对象直接退出
            if (shouldStop || fileInfo.sFileFullNameValue.empty()) {
                break;
            }
            auto now = std::chrono::steady_clock::now();
            double latency = std::chrono::duration<double, std::milli>(now - fileInfo.enqueueTime).count();
            {
                std::unique_lock<std::mutex> lock(latencyMutex);
                totalLatencyMs += latency;
            }
            processedCount++;
            // 通过socket发送到本地服务
            if (socketSendFunc) {
                socketSendFunc(fileInfo);
            }

            {
                // 使用专门的锁来确保控制台输出的原子性
                static std::mutex consoleMutex;
                std::lock_guard<std::mutex> lock(consoleMutex);

                // Windows 控制台：使用 wstring 直接输出到 wcout，避免编码转换问题
                std::wstring filePathW(fileInfo.sFileFullNameValue.begin(), fileInfo.sFileFullNameValue.end());
                std::wcout << L"[NotificationService] socket send: " << filePathW << std::endl;
            }

            g_LogSyncNotificationService.WriteLogContent(
                LOG_INFO,
                "通过socket发送: " + UniConv::GetInstance()->ToUtf8FromUtf16LE(fileInfo.sFileFullNameValue)
            );

        }
    }
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
