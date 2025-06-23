/**
 * @file Lusp_SyncFilesNotificationService.cpp
 * @brief Lusp_SyncFilesNotificationService类的实现文件。
 *
 * 实现了异步监管上传队列、自动pop并通过socket发送到本地服务的核心逻辑。
 */
#include "NotificationService/Lusp_SyncFilesNotificationService.h"
#include <iostream>
#include "log/UniConv.h"
#include "log_headers.h"
#include "SyncUploadQueue/Lusp_SyncUploadQueue.h"
#include "Lusp_SyncUploadQueuePrivate.h"
#include "AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.h"
#include "upload_file_info.pb.h"
#include "asio/asio/io_context.hpp"

const Lusp_AsioIpcConfig Lusp_SyncFilesNotificationService::kDefaultIpcConfig = {};

/**
 * @brief 构造函数，初始化队列指针和统计变量。
 * @param queue 需要被监管的上传队列指针。
 */
Lusp_SyncFilesNotificationService::Lusp_SyncFilesNotificationService(Lusp_SyncUploadQueue& queue)
    : Lusp_SyncFilesNotificationService(queue, Lusp_AsioIpcConfig{}) {}

/**
 * @brief 构造函数，内部自动创建io_context和IPC客户端并管理线程，外部只需传队列和可选配置即可。析构时自动清理。
 */
Lusp_SyncFilesNotificationService::Lusp_SyncFilesNotificationService(Lusp_SyncUploadQueue& queue, const Lusp_AsioIpcConfig& config)
    : queueRef(queue), shouldStop(false), totalLatencyMs(0), processedCount(0), ipcConfig_(config) {
    // 自动管理io_context和IPC客户端
    ioContext_ = std::make_shared<asio::io_context>();
    ipcClient_ = std::make_shared<Lusp_AsioLoopbackIpcClient>(*ioContext_, ipcConfig_);
    ipcClient_->connect();
    // 自动设置socketSendFunc为proto序列化并发送
    setSocketSendFunc([this](const Lusp_SyncUploadFileInfo& info) {
        lusp::PROTOLusp_SyncUploadFileInfo protoMsg = ToProto(info);
        std::string out;
        protoMsg.SerializeToString(&out);
        if (ipcClient_) {
            ipcClient_->send(out);
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
            
            std::cout << "[NotificationService] 通过socket发送: " << UniConv::GetInstance()->ToLocaleFromUtf16LE( fileInfo.sFileFullNameValue )<< std::endl;
            g_LogSyncNotificationService.WriteLogContent(
                LOG_INFO, 
                "通过socket发送: " + UniConv::GetInstance()->ToLocaleFromUtf16LE(fileInfo.sFileFullNameValue)
            );
            
        }
    }
}

void Lusp_SyncFilesNotificationService::setIpcClient(std::shared_ptr<Lusp_AsioLoopbackIpcClient> ipcClient) {
    ipcClient_ = ipcClient;
    // 设置socketSendFunc为自动序列化并发送proto
    setSocketSendFunc([this](const Lusp_SyncUploadFileInfo& info) {
        lusp::PROTOLusp_SyncUploadFileInfo protoMsg = ToProto(info);
        std::string out;
        protoMsg.SerializeToString(&out);
        if (ipcClient_) {
            ipcClient_->send(out);
        }
    });
}

void Lusp_SyncFilesNotificationService::setIpcConfig(const Lusp_AsioIpcConfig& config) {
    ipcConfig_ = config;
}

lusp::PROTOLusp_SyncUploadFileInfo Lusp_SyncFilesNotificationService::ToProto(const Lusp_SyncUploadFileInfo& info) {
    lusp::PROTOLusp_SyncUploadFileInfo proto;
    proto.set_euploadfiletyped(static_cast<lusp::PROTOLusp_UploadFileTyped>(static_cast<int>(info.eUploadFileTyped)));
    auto utf8Str = UniConv::GetInstance()->ToUtf8FromUtf16LE(info.sLanClientDevice);
    printf("LanClientDevice addr: %p, value: %ls\n", info.sLanClientDevice.c_str(), info.sLanClientDevice.c_str());
    printf("utf8Str addr: %p, value: %s\n", utf8Str.c_str(), utf8Str.c_str());
    std::string u8str = "COMHESPHOROS";
 //   int ret;
 //   if (u8str == utf8Str)
 //       std::puts("equal");
 //   else
 //       std::puts("not equal");

 //       
 //   for (unsigned char ch : utf8Str) {
 //       printf("%02X ", ch);
 //   }
 //   printf("\n");
 //   for (unsigned char ch : u8str) {
 //       printf("%02X ", ch);
	//}
	//printf("\n");
    proto.set_slanclientdevice(u8str);

    std::cout << "set_slanClientDevide" << std::endl;
    proto.set_slanclientdevice(UniConv::GetInstance()->ToUtf8FromUtf16LE(info.sLanClientDevice));
    proto.set_ssyncfilesizevalue(static_cast<uint64_t>(info.sSyncFileSizeValue));
    proto.set_sfilefullnamevalue(UniConv::GetInstance()->ToUtf8FromUtf16LE(info.sFileFullNameValue));
    proto.set_sonlyfilenamevalue(UniConv::GetInstance()->ToUtf8FromUtf16LE(info.sOnlyFileNameValue));
    proto.set_sfilerecordtimevalue(info.sFileRecordTimeValue);
    proto.set_sfilemd5valueinfo(info.sFileMd5ValueInfo);
    proto.set_efileexistpolicy(static_cast<lusp::PROTOLusp_FileExistPolicy>(static_cast<int>(info.eFileExistPolicy)));
    proto.set_sauthtokenvalues(info.sAuthTokenValues);
    proto.set_uuploadtimestamp(info.uUploadTimeStamp);
    proto.set_euploadstatusinf(static_cast<lusp::PROTOLusp_UploadStatusInf>(static_cast<int>(info.eUploadStatusInf)));
    proto.set_sdescriptioninfo(UniConv::GetInstance()->ToUtf8FromUtf16LE(info.sDescriptionInfo));
    return proto;
}
