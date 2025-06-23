#ifndef LUSP_SYNCUPLOADQUEUEPRIVATE_H
#define LUSP_SYNCUPLOADQUEUEPRIVATE_H

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <functional>
#include "FileInfo/FileInfo.h"
#include "ThreadSafeRowLockQueue/ThreadSafeRowLockQueue.hpp"

class Lusp_SyncUploadFileInfoHandler;
struct Lusp_SyncUploadFileInfo;

/**
 * @brief 高性能同步上传队列核心实现（PIMPL私有类）
 * 
 * 负责文件上传任务的入队、通知分发、进度回调等核心逻辑。
 * 线程安全，所有通知通过 NotificationService 进行异步处理。
 */
class Lusp_SyncUploadQueuePrivate {
public:
    /**
     * @brief 构造函数，初始化上传队列
     */
    Lusp_SyncUploadQueuePrivate();
    /**
     * @brief 析构函数，清理资源
     */
    ~Lusp_SyncUploadQueuePrivate();
    /**
     * @brief 入队单个文件路径
     * @param filePath 文件全路径
     */
    void pushFile(const std::u16string& filePath);
    /**
     * @brief 批量入队多个文件路径
     * @param filePaths 文件路径列表
     */
    void pushFiles(const std::vector<std::u16string>& filePaths);
    /**
     * @brief 入队文件信息对象
     * @param handler 文件信息处理句柄
     * 只允许传 handler，防止外部绕过校验
     */
    void pushFileInfo(const Lusp_SyncUploadFileInfoHandler& handler);
    /**
     * @brief 清理资源
     */
    void cleanup();
    /**
     * @brief 上传队列，线程安全
     */
    ThreadSafeRowLockQueue<Lusp_SyncUploadFileInfo> uploadQueue;
    /**
     * @brief 业务互斥锁
     */
    std::mutex m_workMutex;
    /**
     * @brief 进度回调函数
     */
    std::function<void(const std::string&, int, const std::string&)> progressCallback;
    std::function<void(const std::u16string&,int,const std::u16string&)> progressCallbackU16;
    /**
     * @brief 
     * 
     */
    std::function<void(const std::string&, bool, const std::string&)>       completedCallback;
    std::function<void(const std::u16string&, bool, const std::u16string&)> completedCallbackU16;
    /**
     * @brief 是否自动开始上传
     */
    std::atomic<bool> m_autoStart;
    /**
     * @brief 队列是否正在运行
     */
    std::atomic<bool> m_isRunning;
    /**
     * @brief 停止标志
     */
    std::atomic<bool> m_shouldStop;
};



#endif // LUSP_SYNCUPLOADQUEUEPRIVATE_H
