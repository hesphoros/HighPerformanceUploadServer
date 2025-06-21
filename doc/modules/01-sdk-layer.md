# SDK层架构文档

## 概述

SDK层是整个系统的核心接口层，提供统一的文件上传接口，管理引用计数，支持本地和Web隔离调用。

## 核心组件

### 1. FileUploadSDK - 统一SDK接口

**职责**：
- 提供统一的文件上传接口
- 管理SDK生命周期
- 支持Web桥接模式
- 处理回调注册

```cpp
class FileUploadSDK {
private:
    static FileUploadSDK* s_instance;
    ReferenceCountManager* m_refCountManager;
    NotificationThread* m_notificationThread;
    IUploadCallback* m_callback;
    WebBridgeServer* m_webBridge;
    bool m_webBridgeEnabled;
    
public:
    // 单例获取
    static FileUploadSDK* getInstance();
    
    // 初始化SDK
    bool initialize(bool enableWebBridge = false);
    
    // 注册回调接口
    void registerCallback(IUploadCallback* callback);
    
    // 发送文件上传请求 - 核心接口
    bool sendFileNotification(const std::string& filePath);
    
    // 获取当前上传状态
    UploadStatus getUploadStatus();
    
    // 清理资源
    void cleanup();
    
private:
    // 初始化引用计数管理器
    bool initializeReferenceCountManager();
    
    // 初始化通知线程
    bool initializeNotificationThread();
    
    // 初始化Web桥接服务器
    bool initializeWebBridge();
};
```

### 2. ReferenceCountManager - 引用计数管理器

**职责**：
- 控制最大50个并发上传
- 管理等待队列
- 处理槽位分配和释放
- 提供状态查询

```cpp
class ReferenceCountManager {
private:
    static const int MAX_CONCURRENT_UPLOADS = 50;
    
    // 核心状态
    std::atomic<int> m_activeCount;                    // 当前活跃数量
    std::map<std::string, UploadSlot> m_activeSlots;  // 活跃槽位信息
    ThreadSafeQueue<UploadTask> m_waitingQueue;        // 等待队列
    
    // 线程安全
    std::mutex m_slotMutex;                           // 槽位操作锁
    std::condition_variable m_slotCondition;          // 槽位变化通知
    
public:
    ReferenceCountManager();
    ~ReferenceCountManager();
    
    // 尝试获取上传槽位
    bool tryAcquireSlot(const UploadTask& task);
    
    // 释放上传槽位
    void releaseSlot(const std::string& fileId, bool success);
    
    // 强制释放槽位（异常情况）
    void forceReleaseSlot(const std::string& fileId);
    
    // 获取当前状态
    UploadStatus getCurrentStatus() const;
    
    // 获取等待队列长度
    size_t getWaitingQueueSize() const;
    
    // 检查是否有可用槽位
    bool hasAvailableSlot() const;
    
private:
    // 启动等待队列中的下一个任务
    void startNextWaitingTask();
    
    // 清理无效槽位
    void cleanupInvalidSlots();
    
    // 生成槽位统计信息
    SlotStatistics generateSlotStatistics() const;
};
```

### 3. UploadSlot - 上传槽位信息

**职责**：
- 存储单个上传任务的状态信息
- 跟踪上传进度
- 记录时间戳

```cpp
struct UploadSlot {
    std::string fileId;                               // 文件唯一标识
    std::string filePath;                            // 文件路径
    std::chrono::steady_clock::time_point startTime; // 开始时间
    std::atomic<int> progress;                       // 当前进度 (0-100)
    std::thread::id threadId;                        // 执行线程ID
    UploadStatus status;                             // 上传状态
    std::string errorMessage;                        // 错误信息
    
    UploadSlot(const std::string& id, const std::string& path)
        : fileId(id), filePath(path), 
          startTime(std::chrono::steady_clock::now()),
          progress(0), status(UploadStatus::Pending) {}
    
    // 获取已用时间
    std::chrono::milliseconds getElapsedTime() const;
    
    // 估算剩余时间
    std::chrono::milliseconds getEstimatedTimeRemaining() const;
    
    // 计算上传速度
    double getUploadSpeed() const;
};
```

### 4. UploadTask - 上传任务

**职责**：
- 封装上传任务信息
- 提供任务唯一标识
- 支持任务优先级

```cpp
struct UploadTask {
    std::string fileId;                  // 文件唯一标识
    std::string filePath;               // 文件完整路径
    size_t fileSize;                    // 文件大小
    TaskPriority priority;              // 任务优先级
    std::chrono::steady_clock::time_point createTime;  // 创建时间
    std::map<std::string, std::string> metadata;       // 扩展元数据
    
    UploadTask(const std::string& path);
    
    // 生成唯一文件ID
    static std::string generateFileId(const std::string& filePath);
    
    // 验证文件是否存在且可读
    bool validateFile() const;
    
    // 获取文件基本信息
    FileInfo getFileInfo() const;
    
    // 比较运算符（用于优先级队列）
    bool operator<(const UploadTask& other) const;
};
```

### 5. UploadStatus - 状态信息

**职责**：
- 提供系统状态快照
- 统计信息汇总
- 性能指标

```cpp
struct UploadStatus {
    // 基本状态
    int activeUploads;          // 当前活跃上传数
    int waitingCount;           // 等待队列长度
    int maxConcurrent;          // 最大并发数
    int totalCompleted;         // 总完成数
    int totalFailed;            // 总失败数
    
    // 活跃槽位详情
    std::vector<UploadSlot> activeSlots;
    
    // 性能统计
    double averageSpeed;        // 平均上传速度 (MB/s)
    std::chrono::milliseconds averageTime;  // 平均完成时间
    
    // 时间戳
    std::chrono::steady_clock::time_point timestamp;
    
    UploadStatus();
    
    // 计算使用率
    double getUsageRate() const;
    
    // 检查是否满负荷
    bool isFullCapacity() const;
    
    // 生成状态报告
    std::string generateStatusReport() const;
    
    // 转换为JSON格式
    std::string toJson() const;
};
```

## 核心流程

### 1. SDK初始化流程

```cpp
// 伪代码：SDK初始化
bool FileUploadSDK::initialize(bool enableWebBridge) {
    // 1. 创建引用计数管理器
    if (!initializeReferenceCountManager()) {
        return false;
    }
    
    // 2. 启动通知线程
    if (!initializeNotificationThread()) {
        return false;
    }
    
    // 3. 可选：启动Web桥接服务器
    if (enableWebBridge) {
        if (!initializeWebBridge()) {
            return false;
        }
        m_webBridgeEnabled = true;
    }
    
    return true;
}
```

### 2. 文件上传请求处理流程

```cpp
// 伪代码：文件上传请求
bool FileUploadSDK::sendFileNotification(const std::string& filePath) {
    // 1. 创建上传任务
    UploadTask task(filePath);
    if (!task.validateFile()) {
        return false;
    }
    
    // 2. 尝试获取槽位
    if (m_refCountManager->tryAcquireSlot(task)) {
        // 3. 立即开始上传
        m_notificationThread->startImmediateUpload(task);
        
        // 4. 通知回调
        if (m_callback) {
            m_callback->onUploadStarted(task.fileId, task.filePath);
        }
        
        return true;
    } else {
        // 5. 加入等待队列
        m_notificationThread->addToQueue(task);
        
        // 6. 通知回调
        if (m_callback) {
            m_callback->onUploadQueued(task.fileId, task.filePath);
        }
        
        return true;
    }
}
```

### 3. 引用计数管理流程

```cpp
// 槽位获取逻辑
bool ReferenceCountManager::tryAcquireSlot(const UploadTask& task) {
    std::unique_lock<std::mutex> lock(m_slotMutex);
    
    // 检查是否有可用槽位
    if (m_activeCount.load() < MAX_CONCURRENT_UPLOADS) {
        // 分配槽位
        m_activeCount++;
        m_activeSlots[task.fileId] = UploadSlot(task.fileId, task.filePath);
        
        return true;
    }
    
    return false;  // 无可用槽位
}

// 槽位释放逻辑
void ReferenceCountManager::releaseSlot(const std::string& fileId, bool success) {
    UploadTask nextTask;
    bool hasWaiting = false;
    
    {
        std::unique_lock<std::mutex> lock(m_slotMutex);
        
        // 清理当前槽位
        auto it = m_activeSlots.find(fileId);
        if (it != m_activeSlots.end()) {
            m_activeSlots.erase(it);
            m_activeCount--;
        }
        
        // 检查等待队列
        if (m_waitingQueue.tryDequeue(nextTask)) {
            hasWaiting = true;
            m_activeCount++;
            m_activeSlots[nextTask.fileId] = UploadSlot(nextTask.fileId, nextTask.filePath);
        }
    }
    
    // 通知槽位变化
    m_slotCondition.notify_all();
    
    // 启动等待任务
    if (hasWaiting) {
        startNextWaitingTask();
    }
}
```

## 接口定义

### 1. IUploadCallback - 回调接口

```cpp
class IUploadCallback {
public:
    virtual ~IUploadCallback() = default;
    
    // 上传开始
    virtual void onUploadStarted(const std::string& fileId, 
                               const std::string& filePath) = 0;
    
    // 上传排队
    virtual void onUploadQueued(const std::string& fileId, 
                              const std::string& filePath) = 0;
    
    // 进度更新（每秒调用）
    virtual void onProgressUpdate(const std::string& fileId, 
                                int progress, 
                                const std::string& message) = 0;
    
    // 上传完成
    virtual void onUploadComplete(const std::string& fileId, 
                                bool success, 
                                const std::string& message) = 0;
    
    // 上传失败
    virtual void onUploadFailed(const std::string& fileId, 
                              const std::string& errorMessage) = 0;
    
    // 状态变化
    virtual void onStatusChanged(const UploadStatus& status) = 0;
};
```

### 2. ThreadSafeQueue - 线程安全队列

```cpp
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    
public:
    // 入队操作（行级锁）
    void enqueue(const T& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(item);
        m_condition.notify_one();
    }
    
    // 出队操作（行级锁）
    bool tryDequeue(T& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        item = m_queue.front();
        m_queue.pop();
        return true;
    }
    
    // 阻塞出队
    void waitAndDequeue(T& item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [this] { return !m_queue.empty(); });
        item = m_queue.front();
        m_queue.pop();
    }
    
    // 获取大小
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }
    
    // 检查是否为空
    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
    
    // 清空队列
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::queue<T> empty;
        std::swap(m_queue, empty);
    }
};
```

## 使用示例

### 1. 基本使用

```cpp
// 初始化SDK
auto sdk = FileUploadSDK::getInstance();
sdk->initialize();

// 注册回调
class MyCallback : public IUploadCallback {
public:
    void onUploadStarted(const std::string& fileId, const std::string& filePath) override {
        std::cout << "开始上传: " << filePath << std::endl;
    }
    
    void onProgressUpdate(const std::string& fileId, int progress, const std::string& message) override {
        std::cout << "进度: " << progress << "%" << std::endl;
    }
    
    void onUploadComplete(const std::string& fileId, bool success, const std::string& message) override {
        std::cout << "上传完成: " << (success ? "成功" : "失败") << std::endl;
    }
    
    // ... 其他回调实现
};

MyCallback callback;
sdk->registerCallback(&callback);

// 发送文件
sdk->sendFileNotification("C:/test/file1.txt");
sdk->sendFileNotification("C:/test/file2.txt");

// 查看状态
auto status = sdk->getUploadStatus();
std::cout << "活跃上传: " << status.activeUploads << std::endl;
std::cout << "等待队列: " << status.waitingCount << std::endl;
```

### 2. Web桥接模式

```cpp
// 启用Web桥接
auto sdk = FileUploadSDK::getInstance();
sdk->initialize(true);  // 启用Web桥接

// Web客户端可通过HTTP调用：
// POST http://localhost:8080/upload
// WebSocket连接：ws://localhost:8081/progress
```

## 配置选项

```cpp
struct SDKConfig {
    int maxConcurrentUploads = 50;      // 最大并发数
    int queueMaxSize = 1000;            // 等待队列最大长度
    int progressUpdateInterval = 1000;   // 进度更新间隔（毫秒）
    bool enableWebBridge = false;       // 启用Web桥接
    int webHttpPort = 8080;             // Web HTTP端口
    int webSocketPort = 8081;           // WebSocket端口
    std::string logLevel = "INFO";      // 日志级别
};
```

## 性能指标

- **最大并发**：50个文件同时上传
- **队列容量**：1000个等待任务
- **进度更新**：每秒1次回调
- **内存使用**：约10MB（50个活跃槽位）
- **CPU使用**：单核10%以下
- **响应延迟**：< 1ms（本地调用）

## 注意事项

1. **线程安全**：所有接口都是线程安全的
2. **资源管理**：SDK负责管理所有内部资源
3. **错误处理**：提供详细的错误信息和回调
4. **兼容性**：支持Windows 7及以上版本
5. **性能优化**：使用引用计数避免频繁的线程创建/销毁

---

*文档版本：v1.0*  
*更新时间：2025年6月21日*
