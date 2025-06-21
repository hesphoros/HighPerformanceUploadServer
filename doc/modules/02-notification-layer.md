# 通知层架构文档

## 概述

通知层是SDK层和本地服务层之间的桥梁，负责维护行级锁队列，启动通知线程，并通过进程间通信与本地服务进行交互。严格按照原始需求"客户端只干了一件事，启动了一个发送通知线程"的设计。

## 核心组件

### 1. NotificationThread - 通知线程

**职责**：
- 维护线程安全的通知队列
- 监听队列变化，使用条件变量唤醒
- 与本地服务进行进程间通信
- 处理上传完成事件

```cpp
class NotificationThread {
private:
    // 队列和同步
    ThreadSafeQueue<FileInfo> m_notificationQueue;    // 通知队列
    std::thread m_thread;                             // 通知线程
    std::atomic<bool> m_running;                      // 运行状态
    std::atomic<bool> m_paused;                       // 暂停状态
    
    // 组件依赖
    ReferenceCountManager* m_refCountManager;         // 引用计数管理器
    LocalServiceClient* m_localServiceClient;        // 本地服务客户端
    IUploadCallback* m_callback;                     // 回调接口
    
    // 统计信息
    std::atomic<uint64_t> m_processedCount;          // 已处理数量
    std::atomic<uint64_t> m_errorCount;              // 错误数量
    
public:
    NotificationThread(ReferenceCountManager* refManager, 
                      IUploadCallback* callback);
    ~NotificationThread();
    
    // 线程控制
    bool start();
    void stop();
    void pause();
    void resume();
    
    // 队列操作
    bool addFileToQueue(const std::string& filePath);
    bool addFileToQueue(const FileInfo& fileInfo);
    
    // 立即启动上传（有可用槽位时）
    bool startImmediateUpload(const UploadTask& task);
    
    // 获取统计信息
    NotificationStats getStatistics() const;
    
    // 清空队列
    void clearQueue();
    
private:
    // 线程主循环
    void threadMain();
    
    // 处理通知队列
    void processNotificationQueue();
    
    // 发送通知到本地服务
    bool sendNotificationToLocalService(const FileInfo& fileInfo);
    
    // 处理上传完成事件
    void handleUploadComplete(const std::string& fileId, bool success, const std::string& message);
    
    // 错误处理
    void handleError(const std::string& errorMessage);
    
    // 生成文件信息
    FileInfo generateFileInfo(const std::string& filePath);
};
```

### 2. ThreadSafeQueue - 行级锁队列

**职责**：
- 实现行级锁机制，只锁入队和出队操作
- 封装条件变量，有数据进来就唤醒线程
- 提供高性能的队列操作

```cpp
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> m_queue;
    mutable std::shared_mutex m_rwMutex;      // 读写锁
    std::condition_variable_any m_condition;  // 条件变量
    std::atomic<size_t> m_size;              // 原子计数器
    size_t m_maxSize;                        // 最大容量
    
public:
    explicit ThreadSafeQueue(size_t maxSize = 1000);
    
    // 入队操作 - 行级锁
    bool enqueue(const T& item);
    bool enqueue(T&& item);
    
    // 批量入队
    bool enqueueBatch(const std::vector<T>& items);
    
    // 出队操作 - 行级锁
    bool tryDequeue(T& item);
    
    // 阻塞出队 - 条件变量唤醒
    void waitAndDequeue(T& item);
    
    // 超时出队
    bool waitAndDequeue(T& item, std::chrono::milliseconds timeout);
    
    // 批量出队
    size_t dequeueBatch(std::vector<T>& items, size_t maxCount);
    
    // 状态查询
    size_t size() const;
    bool empty() const;
    bool full() const;
    size_t capacity() const;
    
    // 队列管理
    void clear();
    void setMaxSize(size_t maxSize);
    
    // 统计信息
    QueueStatistics getStatistics() const;
    
private:
    // 内部工具方法
    bool isFullInternal() const;
    void notifyWaiters();
};

// 队列统计信息
struct QueueStatistics {
    size_t currentSize;         // 当前大小
    size_t maxSize;            // 最大容量
    uint64_t totalEnqueued;    // 总入队数
    uint64_t totalDequeued;    // 总出队数
    uint64_t totalDropped;     // 总丢弃数
    double utilizationRate;    // 使用率
    std::chrono::milliseconds averageWaitTime;  // 平均等待时间
};
```

### 3. LocalServiceClient - 本地服务客户端

**职责**：
- 通过Named Pipe与本地服务进程通信
- 发送文件上传通知
- 接收上传完成通知
- 处理连接重试和错误恢复

```cpp
class LocalServiceClient {
private:
    HANDLE m_pipe;                    // Named Pipe句柄
    std::string m_pipeName;           // 管道名称
    std::atomic<bool> m_connected;    // 连接状态
    std::mutex m_sendMutex;          // 发送锁
    std::mutex m_receiveMutex;       // 接收锁
    
    // 重试配置
    int m_maxRetries;
    std::chrono::milliseconds m_retryInterval;
    
    // 统计信息
    std::atomic<uint64_t> m_messagesSent;
    std::atomic<uint64_t> m_messagesReceived;
    std::atomic<uint64_t> m_connectionFailures;
    
public:
    LocalServiceClient(const std::string& pipeName = "\\\\.\\pipe\\UploadService");
    ~LocalServiceClient();
    
    // 连接管理
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    // 消息发送
    bool sendUploadNotification(const FileInfo& fileInfo);
    bool sendControlMessage(const ControlMessage& message);
    
    // 消息接收
    bool receiveMessage(ServiceMessage& message);
    bool receiveMessage(ServiceMessage& message, std::chrono::milliseconds timeout);
    
    // 配置
    void setRetryPolicy(int maxRetries, std::chrono::milliseconds interval);
    
    // 统计信息
    ClientStatistics getStatistics() const;
    
private:
    // 内部实现
    bool connectInternal();
    bool sendMessageInternal(const void* data, size_t size);
    bool receiveMessageInternal(void* buffer, size_t bufferSize, size_t& receivedSize);
    
    // 重试机制
    bool retryWithBackoff(std::function<bool()> operation);
    
    // 错误处理
    void handleConnectionError();
    std::string getLastErrorString();
};
```

### 4. FileInfo - 文件信息结构

**职责**：
- 封装文件基本信息
- 提供文件验证功能
- 支持序列化/反序列化

```cpp
struct FileInfo {
    std::string fileId;              // 文件唯一标识
    std::string filePath;            // 文件完整路径
    std::string fileName;            // 文件名
    size_t fileSize;                 // 文件大小（字节）
    std::string fileHash;            // 文件哈希值（MD5）
    std::string mimeType;            // MIME类型
    std::chrono::file_time_type lastModified;  // 最后修改时间
    
    // 元数据
    std::map<std::string, std::string> metadata;
    
    // 上传配置
    TaskPriority priority;           // 任务优先级
    int chunkSize;                   // 分块大小
    bool enableResume;               // 支持断点续传
    
    FileInfo();
    explicit FileInfo(const std::string& path);
    
    // 文件操作
    bool validateFile() const;
    bool calculateHash();
    std::string detectMimeType() const;
    
    // 序列化
    std::string serialize() const;
    bool deserialize(const std::string& data);
    std::vector<uint8_t> toBinary() const;
    bool fromBinary(const std::vector<uint8_t>& data);
    
    // 工具方法
    std::string getFileExtension() const;
    bool isValidPath() const;
    double getSizeInMB() const;
    
    // 比较运算符
    bool operator==(const FileInfo& other) const;
    bool operator<(const FileInfo& other) const;
};
```

### 5. NotificationStats - 通知统计

**职责**：
- 收集通知线程性能数据
- 提供诊断和监控信息

```cpp
struct NotificationStats {
    // 基本统计
    uint64_t totalProcessed;         // 总处理数量
    uint64_t totalErrors;            // 总错误数量
    uint64_t currentQueueSize;       // 当前队列大小
    
    // 性能指标
    double processRate;              // 处理速率（个/秒）
    double errorRate;                // 错误率
    std::chrono::milliseconds averageProcessTime;  // 平均处理时间
    
    // 时间统计
    std::chrono::steady_clock::time_point startTime;    // 启动时间
    std::chrono::milliseconds totalRunTime;             // 总运行时间
    std::chrono::milliseconds totalIdleTime;            // 总空闲时间
    
    // 队列统计
    QueueStatistics queueStats;
    
    NotificationStats();
    
    // 计算方法
    double getUptimeHours() const;
    double getEfficiencyRate() const;
    std::string generateReport() const;
    
    // 序列化
    std::string toJson() const;
};
```

## 核心流程

### 1. 通知线程初始化流程

```cpp
bool NotificationThread::start() {
    if (m_running.load()) {
        return false;  // 已经在运行
    }
    
    // 1. 初始化本地服务客户端
    if (!m_localServiceClient->connect()) {
        return false;
    }
    
    // 2. 设置运行状态
    m_running.store(true);
    m_paused.store(false);
    
    // 3. 启动线程
    m_thread = std::thread(&NotificationThread::threadMain, this);
    
    return true;
}

void NotificationThread::threadMain() {
    while (m_running.load()) {
        try {
            // 检查暂停状态
            if (m_paused.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            // 处理通知队列
            processNotificationQueue();
            
        } catch (const std::exception& e) {
            handleError(e.what());
        }
    }
}
```

### 2. 队列处理流程

```cpp
void NotificationThread::processNotificationQueue() {
    FileInfo fileInfo;
    
    // 从队列中获取文件信息 - 条件变量等待
    if (m_notificationQueue.waitAndDequeue(fileInfo, std::chrono::milliseconds(1000))) {
        // 发送通知到本地服务
        if (sendNotificationToLocalService(fileInfo)) {
            m_processedCount++;
            
            // 回调通知
            if (m_callback) {
                m_callback->onUploadQueued(fileInfo.fileId, fileInfo.filePath);
            }
        } else {
            m_errorCount++;
            handleError("Failed to send notification to local service");
        }
    }
}

bool NotificationThread::sendNotificationToLocalService(const FileInfo& fileInfo) {
    // 序列化文件信息
    std::string serializedData = fileInfo.serialize();
    
    // 发送到本地服务
    return m_localServiceClient->sendUploadNotification(fileInfo);
}
```

### 3. 文件入队流程

```cpp
bool NotificationThread::addFileToQueue(const std::string& filePath) {
    // 1. 生成文件信息
    FileInfo fileInfo = generateFileInfo(filePath);
    
    // 2. 验证文件
    if (!fileInfo.validateFile()) {
        if (m_callback) {
            m_callback->onUploadFailed(fileInfo.fileId, "File validation failed");
        }
        return false;
    }
    
    // 3. 添加到队列 - 行级锁操作
    if (m_notificationQueue.enqueue(fileInfo)) {
        return true;
    } else {
        if (m_callback) {
            m_callback->onUploadFailed(fileInfo.fileId, "Queue is full");
        }
        return false;
    }
}

FileInfo NotificationThread::generateFileInfo(const std::string& filePath) {
    FileInfo info(filePath);
    
    // 计算文件哈希
    info.calculateHash();
    
    // 检测MIME类型
    info.mimeType = info.detectMimeType();
    
    // 设置默认优先级
    info.priority = TaskPriority::Normal;
    
    return info;
}
```

### 4. 行级锁队列实现

```cpp
template<typename T>
bool ThreadSafeQueue<T>::enqueue(const T& item) {
    {
        // 入队行级锁
        std::unique_lock<std::shared_mutex> lock(m_rwMutex);
        
        if (isFullInternal()) {
            return false;  // 队列已满
        }
        
        m_queue.push(item);
        m_size++;
    }
    
    // 通知等待的线程 - 条件变量唤醒
    m_condition.notify_one();
    return true;
}

template<typename T>
bool ThreadSafeQueue<T>::tryDequeue(T& item) {
    // 出队行级锁
    std::unique_lock<std::shared_mutex> lock(m_rwMutex);
    
    if (m_queue.empty()) {
        return false;
    }
    
    item = m_queue.front();
    m_queue.pop();
    m_size--;
    
    return true;
}

template<typename T>
void ThreadSafeQueue<T>::waitAndDequeue(T& item) {
    std::unique_lock<std::shared_mutex> lock(m_rwMutex);
    
    // 条件变量等待 - 有数据进来就唤醒
    m_condition.wait(lock, [this] { return !m_queue.empty(); });
    
    item = m_queue.front();
    m_queue.pop();
    m_size--;
}
```

## 进程间通信

### 1. Named Pipe通信协议

```cpp
// 消息头结构
struct MessageHeader {
    uint32_t messageType;    // 消息类型
    uint32_t messageSize;    // 消息大小
    uint32_t checksum;       // 校验和
    uint32_t sequence;       // 序列号
};

// 消息类型定义
enum class MessageType : uint32_t {
    UPLOAD_NOTIFICATION = 1,    // 上传通知
    UPLOAD_COMPLETE = 2,        // 上传完成
    UPLOAD_PROGRESS = 3,        // 上传进度
    CONTROL_MESSAGE = 4,        // 控制消息
    HEARTBEAT = 5               // 心跳消息
};

// 上传通知消息
struct UploadNotificationMessage {
    MessageHeader header;
    char fileId[64];
    char filePath[512];
    uint64_t fileSize;
    char fileHash[33];
    uint32_t priority;
    char metadata[256];
};
```

### 2. 消息序列化

```cpp
class MessageSerializer {
public:
    static std::vector<uint8_t> serialize(const UploadNotificationMessage& message);
    static bool deserialize(const std::vector<uint8_t>& data, UploadNotificationMessage& message);
    
    static uint32_t calculateChecksum(const void* data, size_t size);
    static bool validateChecksum(const MessageHeader& header, const void* data);
    
private:
    static void writeUint32(std::vector<uint8_t>& buffer, uint32_t value);
    static void writeUint64(std::vector<uint8_t>& buffer, uint64_t value);
    static void writeString(std::vector<uint8_t>& buffer, const std::string& str, size_t maxLength);
};
```

## 错误处理和重试机制

### 1. 连接重试策略

```cpp
class RetryPolicy {
private:
    int m_maxRetries;
    std::chrono::milliseconds m_baseDelay;
    double m_backoffMultiplier;
    std::chrono::milliseconds m_maxDelay;
    
public:
    RetryPolicy(int maxRetries = 5, 
               std::chrono::milliseconds baseDelay = std::chrono::milliseconds(100),
               double backoffMultiplier = 2.0,
               std::chrono::milliseconds maxDelay = std::chrono::milliseconds(5000));
    
    bool shouldRetry(int attemptCount) const;
    std::chrono::milliseconds getDelay(int attemptCount) const;
    void reset();
};
```

### 2. 错误恢复机制

```cpp
void NotificationThread::handleError(const std::string& errorMessage) {
    m_errorCount++;
    
    // 记录错误日志
    LogManager::getInstance()->logError("NotificationThread", errorMessage);
    
    // 尝试重新连接本地服务
    if (!m_localServiceClient->isConnected()) {
        if (m_localServiceClient->connect()) {
            LogManager::getInstance()->logInfo("NotificationThread", "Reconnected to local service");
        }
    }
    
    // 错误率过高时暂停
    if (getErrorRate() > 0.5) {
        pause();
        
        // 通知回调
        if (m_callback) {
            m_callback->onStatusChanged(UploadStatus{});
        }
        
        // 等待后恢复
        std::this_thread::sleep_for(std::chrono::seconds(5));
        resume();
    }
}
```

## 性能优化

### 1. 批量处理优化

```cpp
void NotificationThread::processBatch() {
    std::vector<FileInfo> batch;
    
    // 批量出队
    size_t batchSize = m_notificationQueue.dequeueBatch(batch, 10);
    
    if (batchSize > 0) {
        // 批量发送到本地服务
        for (const auto& fileInfo : batch) {
            sendNotificationToLocalService(fileInfo);
        }
        
        m_processedCount += batchSize;
    }
}
```

### 2. 内存池优化

```cpp
class MessagePool {
private:
    std::queue<std::unique_ptr<UploadNotificationMessage>> m_pool;
    std::mutex m_poolMutex;
    size_t m_maxPoolSize;
    
public:
    std::unique_ptr<UploadNotificationMessage> acquire();
    void release(std::unique_ptr<UploadNotificationMessage> message);
    
private:
    std::unique_ptr<UploadNotificationMessage> createNew();
};
```

## 配置选项

```cpp
struct NotificationConfig {
    size_t queueMaxSize = 1000;                     // 队列最大容量
    std::chrono::milliseconds queueTimeout = std::chrono::milliseconds(1000);  // 队列超时
    int maxRetries = 5;                             // 最大重试次数
    std::chrono::milliseconds retryInterval = std::chrono::milliseconds(1000); // 重试间隔
    std::string pipeName = "\\\\.\\pipe\\UploadService";  // 管道名称
    bool enableBatchProcessing = true;              // 启用批量处理
    size_t batchSize = 10;                         // 批量大小
    bool enableMessagePool = true;                  // 启用消息池
    size_t messagePoolSize = 100;                  // 消息池大小
};
```

## 监控和诊断

### 1. 性能监控

```cpp
class NotificationMonitor {
private:
    std::chrono::steady_clock::time_point m_startTime;
    std::atomic<uint64_t> m_totalProcessed;
    std::atomic<uint64_t> m_totalErrors;
    
public:
    void recordProcessed();
    void recordError();
    
    double getProcessRate() const;
    double getErrorRate() const;
    std::chrono::milliseconds getUptime() const;
    
    std::string generateReport() const;
};
```

### 2. 诊断工具

```cpp
class NotificationDiagnostics {
public:
    static bool testPipeConnection(const std::string& pipeName);
    static QueueHealth checkQueueHealth(const ThreadSafeQueue<FileInfo>& queue);
    static std::string generateHealthReport(const NotificationThread& thread);
    
private:
    enum class QueueHealth { Healthy, Warning, Critical };
};
```

## 使用示例

```cpp
// 创建通知线程
auto refCountManager = std::make_unique<ReferenceCountManager>();
auto callback = std::make_unique<MyCallback>();
auto notificationThread = std::make_unique<NotificationThread>(refCountManager.get(), callback.get());

// 启动线程
if (notificationThread->start()) {
    // 添加文件到队列
    notificationThread->addFileToQueue("C:/test/file1.txt");
    notificationThread->addFileToQueue("C:/test/file2.txt");
    
    // 查看统计
    auto stats = notificationThread->getStatistics();
    std::cout << "处理速率: " << stats.processRate << " 个/秒" << std::endl;
    std::cout << "队列大小: " << stats.currentQueueSize << std::endl;
}

// 停止线程
notificationThread->stop();
```

---

*文档版本：v1.0*  
*更新时间：2025年6月21日*
