#pragma once
#include <WinSock2.h>
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <functional>
#include <chrono>
#include <mutex>
#include "ThreadSafeRowLockQueue/ThreadSafeRowLockQueue.hpp"
#include "FileInfo/FileInfo.h"
#include "SyncUploadQueue/Lusp_SyncUploadQueue.h"
#include "AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.h"

/**
 * @class Lusp_SyncFilesNotificationService
 * @brief 负责异步监管上传队列并通过socket与本地服务通信的通知服务。
 * 
 * 该服务通过独立线程持续监控上传队列（ThreadSafeRowLockQueue），
 * 队列有内容时自动pop并通过socket回调发送到本地服务，实现UI与上传解耦。
 * 支持处理统计、延迟监控、诊断信息导出等。
 */
class Lusp_SyncFilesNotificationService {
public:
    /**
     * @typedef UploadQueueType
     * @brief 上传队列类型，线程安全。
     */
    using UploadQueueType = ThreadSafeRowLockQueue<Lusp_SyncUploadFileInfo>;
    /**
     * @typedef SocketSendFunc
     * @brief 发送到本地服务的回调函数类型。
     */
    using SocketSendFunc = std::function<void(const Lusp_SyncUploadFileInfo&)>;

    /**
     * @brief 构造函数，传入全局上传队列引用。
     * @param queue 需要被监管的上传队列引用。
     */
    explicit Lusp_SyncFilesNotificationService(Lusp_SyncUploadQueue& queue);
    /**
     * @brief 推荐构造：自动管理IPC，外部只需传队列和可选配置。
     */
    Lusp_SyncFilesNotificationService(Lusp_SyncUploadQueue& queue, const Lusp_AsioIpcConfig& config);
    /**
     * @brief 析构函数，自动停止监管线程。
     */
    ~Lusp_SyncFilesNotificationService();

    // 禁止拷贝
    Lusp_SyncFilesNotificationService(const Lusp_SyncFilesNotificationService&) = delete;
    Lusp_SyncFilesNotificationService& operator=(const Lusp_SyncFilesNotificationService&) = delete;

    /**
     * @brief 启动异步监管线程，自动pop队列并发送到本地服务。
     */
    void start();
    /**
     * @brief 停止监管线程，安全退出。
     */
    void stop();
    /**
     * @brief 设置socket通信回调。
     * @param func 发送到本地服务的回调函数。
     */
    void setSocketSendFunc(SocketSendFunc func);
    /**
     * @brief 获取已处理任务数。
     * @return 已处理的文件数。
     */
    size_t getProcessedCount() const;
    /**
     * @brief 获取平均处理延迟（ms）。
     * @return 平均延迟，单位毫秒。
     */
    double getAverageLatencyMs() const;
    /**
     * @brief 导出当前服务状态诊断信息。
     * @return 状态字符串。
     */
    std::string dumpStatus() const;
    /**
     * @brief 设置IPC客户端（用于自动发送proto消息）。
     */
    void setIpcClient(std::shared_ptr<Lusp_AsioLoopbackIpcClient> ipcClient);
    /**
     * @brief 设置IPC配置（可选，便于构造内部IPC客户端）。
     */
    void setIpcConfig(const Lusp_AsioIpcConfig& config);
private:
    /**
     * @brief 监管线程主循环，阻塞等待队列有内容时自动pop并处理。
     */
    void notificationLoop();
    Lusp_SyncUploadQueue& queueRef; // 保存队列引用
    std::thread notifyThread; ///< 监管线程
    std::atomic<bool> shouldStop{false}; ///< 停止标志
    SocketSendFunc socketSendFunc; ///< socket通信回调函数
    // 性能统计
    std::atomic<size_t> processedCount{0}; ///< 已处理任务数
    double totalLatencyMs{0}; ///< 总处理延迟
    mutable std::mutex latencyMutex; ///< 延迟统计锁
    std::shared_ptr<Lusp_AsioLoopbackIpcClient> ipcClient_;
    Lusp_AsioIpcConfig ipcConfig_;
    std::shared_ptr<asio::io_context> ioContext_;
    std::thread ioThread_;
    
    std::string ToFlatBuffer(const Lusp_SyncUploadFileInfo& info);
    static const Lusp_AsioIpcConfig kDefaultIpcConfig;
};


