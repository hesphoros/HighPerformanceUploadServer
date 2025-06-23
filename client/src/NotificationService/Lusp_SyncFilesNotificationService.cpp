/**
 * @file Lusp_SyncFilesNotificationService.cpp
 * @brief Lusp_SyncFilesNotificationService类的实现文件。
 *
 * 实现了异步监管上传队列、自动pop并通过socket发送到本地服务的核心逻辑。
 */
#include "NotificationService/Lusp_SyncFilesNotificationService.h"
#include <iostream>
#include "log/UniConv.h"

/**
 * @brief 构造函数，初始化队列指针和统计变量。
 * @param queue 需要被监管的上传队列指针。
 */
Lusp_SyncFilesNotificationService::Lusp_SyncFilesNotificationService(UploadQueueType* queue)
    : uploadQueue(queue), shouldStop(false), totalLatencyMs(0), processedCount(0) {}

/**
 * @brief 析构函数，自动停止监管线程。
 */
Lusp_SyncFilesNotificationService::~Lusp_SyncFilesNotificationService() {
    stop();
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
        if (uploadQueue && uploadQueue->waitAndPop(fileInfo)) {
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
            } else {
                std::cout << "[NotificationService] 通过socket发送: " << UniConv::GetInstance()->ToLocaleFromUtf16LE( fileInfo.sFileFullNameValue )<< std::endl;
            }
        }
    }
}
