#ifndef INCLUDED_LUSP_UPLOAD_QUEUE_H
#define INCLUDED_LUSP_UPLOAD_QUEUE_H

class Lusp_SyncUploadQueuePrivate;
class Lusp_SyncFilesNotificationService; // 前置声明

#ifdef _MSC_VER
// utf-8
#pragma execution_character_set("utf-8")
#endif // _MSC_VER

#include <QString>
#include <QList>
#include <QString>
#include <QStringList>
#include <QObject>
#include <functional>
#include <string>
#include <list>
#include "FileInfo/FileInfo.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include "ThreadSafeRowLockQueue/ThreadSafeRowLockQueue.hpp"

/**
 * @brief 高性能行级锁上传队列 - 使用标准C++实现
 * 
 * 核心设计：
 * - 入队锁和出队锁分离，UI线程和通知线程不互相阻塞
 * - 条件变量精确唤醒机制
 * - 纯C++标准库实现
 */
class Lusp_SyncUploadQueue {
public:
    // 进度回调函数类型
    using ProgressCallback = std::function<void(const std::string& filePath, int percentage, const std::string& status)>;
    using CompletedCallback = std::function<void(const std::string& filePath, bool success, const std::string& message)>;

    // 获取全局单例
    static Lusp_SyncUploadQueue& instance();

   
    void push(const std::u16string& filePath);                    // 推送单个文件
    void push(const std::vector<std::u16string>& filePaths);      // 推送多个文件
    void push(const Lusp_SyncUploadFileInfo& fileInfo);       // 推送文件信息对象
    
   
    void setProgressCallback(ProgressCallback callback);
    void setCompletedCallback(CompletedCallback callback);
    void setAutoStart(bool autoStart = true);
    
  
    size_t pendingCount() const;
    bool isActive() const;
    bool empty() const;

    friend class Lusp_SyncFilesNotificationService;
private:
    Lusp_SyncUploadQueue();
    ~Lusp_SyncUploadQueue();
    
    // 禁用拷贝
    Lusp_SyncUploadQueue(const Lusp_SyncUploadQueue&) = delete;
    Lusp_SyncUploadQueue& operator=(const Lusp_SyncUploadQueue&) = delete;

private:
    std::unique_ptr<Lusp_SyncUploadQueuePrivate> d;
};


//namespace Upload {
//    inline void push(const std::u16string& filePath) {
//        Lusp_SyncUploadQueue::instance().push(filePath);
//    }
//    
//    inline void push(const std::vector<std::u16string>& filePaths) {
//        Lusp_SyncUploadQueue::instance().push(filePaths);
//    }
//    
//    inline void push(const Lusp_SyncUploadFileInfo& fileInfo) {
//        Lusp_SyncUploadQueue::instance().push(fileInfo);
//    }
//}

#endif // INCLUDED_LUSP_UPLOAD_QUEUE_H
