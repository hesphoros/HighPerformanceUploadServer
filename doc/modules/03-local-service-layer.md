# 本地服务层架构文档

## 概述

本地服务层是独立的服务进程，负责接收通知线程的文件上传请求，管理上传队列，启动后台上传线程。严格按照原始需求"由本地服务将要上传的文件写入队列由后台服务进行上传"的设计。

## 核心组件

### 1. LocalUploadService - 本地上传服务

**职责**：
- 作为独立服务进程运行
- 接收来自通知线程的上传请求
- 管理上传队列和线程池
- 启动后台上传线程
- 提供进度报告服务

```cpp
class LocalUploadService {
private:
    // 服务状态
    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
    ServiceStatus m_status;
    
    // 核心组件
    NamedPipeServer* m_pipeServer;                    // 进程间通信服务器
    UploadQueueManager* m_queueManager;              // 上传队列管理器
    ThreadPoolManager* m_threadPoolManager;          // 线程池管理器
    ProgressReporter* m_progressReporter;            // 进度报告器
    
    // 活跃上传管理
    std::map<std::string, BackgroundUploader*> m_activeUploaders;
    std::mutex m_uploadersMutex;
    
    // 统计信息
    std::atomic<uint64_t> m_totalProcessed;
    std::atomic<uint64_t> m_totalCompleted;
    std::atomic<uint64_t> m_totalFailed;
    
public:
    LocalUploadService();
    ~LocalUploadService();
    
    // 服务控制
    bool start();
    void stop();
    void pause();
    void resume();
    
    // 服务状态
    ServiceStatus getStatus() const;
    ServiceStatistics getStatistics() const;
    
    // 配置管理
    bool loadConfiguration(const std::string& configFile);
    void setConfiguration(const ServiceConfig& config);
    
private:
    // 初始化组件
    bool initializeComponents();
    void cleanupComponents();
    
    // 处理上传请求
    void handleUploadRequest(const UploadNotificationMessage& message);
    
    // 启动后台上传
    bool startBackgroundUpload(const UploadTask& task);
    
    // 处理上传完成
    void handleUploadComplete(const std::string& fileId, bool success, const std::string& message);
    
    // 清理完成的上传器
    void cleanupCompletedUploader(const std::string& fileId);
    
    // 服务主循环
    void serviceMainLoop();
    
    // 错误处理
    void handleServiceError(const std::string& errorMessage);
};
```

### 2. NamedPipeServer - 命名管道服务器

**职责**：
- 创建和管理Named Pipe
- 接收来自通知线程的消息
- 处理多个客户端连接
- 消息序列化和反序列化

```cpp
class NamedPipeServer {
private:
    HANDLE m_pipe;                           // 主管道句柄
    std::string m_pipeName;                  // 管道名称
    std::vector<HANDLE> m_clientPipes;       // 客户端管道列表
    std::thread m_serverThread;              // 服务器线程
    std::atomic<bool> m_running;             // 运行状态
    
    // 消息处理
    MessageProcessor* m_messageProcessor;    // 消息处理器
    std::queue<ReceivedMessage> m_messageQueue;  // 消息队列
    std::mutex m_messageQueueMutex;
    
    // 连接管理
    std::atomic<int> m_activeConnections;
    int m_maxConnections;
    
public:
    NamedPipeServer(const std::string& pipeName = "\\\\.\\pipe\\UploadService");
    ~NamedPipeServer();
    
    // 服务器控制
    bool start();
    void stop();
    bool isRunning() const;
    
    // 连接管理
    int getActiveConnections() const;
    void setMaxConnections(int maxConnections);
    
    // 消息处理回调
    void setMessageProcessor(MessageProcessor* processor);
    
    // 统计信息
    ServerStatistics getStatistics() const;
    
private:
    // 创建管道
    bool createPipe();
    
    // 服务器主循环
    void serverMainLoop();
    
    // 处理客户端连接
    void handleClientConnection(HANDLE clientPipe);
    
    // 接收消息
    bool receiveMessage(HANDLE clientPipe, ReceivedMessage& message);
    
    // 发送响应
    bool sendResponse(HANDLE clientPipe, const ResponseMessage& response);
    
    // 清理客户端连接
    void cleanupClientConnection(HANDLE clientPipe);
    
    // 错误处理
    void handlePipeError(const std::string& errorMessage);
};
```

### 3. UploadQueueManager - 上传队列管理器

**职责**：
- 管理文件上传队列
- 实现优先级队列
- 提供队列统计和监控
- 支持队列持久化

```cpp
class UploadQueueManager {
private:
    // 队列存储
    std::priority_queue<UploadTask> m_priorityQueue;     // 优先级队列
    std::queue<UploadTask> m_normalQueue;                // 普通队列
    ThreadSafeQueue<UploadTask> m_processingQueue;       // 处理队列
    
    // 线程安全
    std::shared_mutex m_queueMutex;
    std::condition_variable_any m_queueCondition;
    
    // 队列配置
    size_t m_maxQueueSize;
    QueuePolicy m_queuePolicy;
    
    // 持久化
    QueuePersistence* m_persistence;
    bool m_enablePersistence;
    
    // 统计信息
    std::atomic<uint64_t> m_totalEnqueued;
    std::atomic<uint64_t> m_totalDequeued;
    std::atomic<uint64_t> m_totalDropped;
    
public:
    UploadQueueManager(size_t maxSize = 1000);
    ~UploadQueueManager();
    
    // 队列操作
    bool enqueue(const UploadTask& task);
    bool enqueue(const UploadTask& task, TaskPriority priority);
    bool tryDequeue(UploadTask& task);
    void waitAndDequeue(UploadTask& task);
    
    // 批量操作
    size_t enqueueBatch(const std::vector<UploadTask>& tasks);
    size_t dequeueBatch(std::vector<UploadTask>& tasks, size_t maxCount);
    
    // 队列状态
    size_t size() const;
    size_t priorityQueueSize() const;
    size_t normalQueueSize() const;
    bool empty() const;
    bool full() const;
    
    // 队列管理
    void clear();
    void clearByPriority(TaskPriority priority);
    void removeTask(const std::string& fileId);
    
    // 持久化控制
    bool enablePersistence(const std::string& persistenceFile);
    void disablePersistence();
    bool saveQueue();
    bool loadQueue();
    
    // 统计信息
    QueueStatistics getStatistics() const;
    
    // 配置管理
    void setQueuePolicy(const QueuePolicy& policy);
    void setMaxSize(size_t maxSize);
    
private:
    // 内部队列操作
    bool enqueueInternal(const UploadTask& task, TaskPriority priority);
    bool dequeueFromPriorityQueue(UploadTask& task);
    bool dequeueFromNormalQueue(UploadTask& task);
    
    // 队列策略处理
    bool shouldDropTask(const UploadTask& task);
    void applyQueuePolicy();
    
    // 清理过期任务
    void cleanupExpiredTasks();
};
```

### 4. ThreadPoolManager - 线程池管理器

**职责**：
- 管理后台上传线程池
- 控制线程数量（最大50个）
- 监控线程状态和性能
- 提供线程调度策略

```cpp
class ThreadPoolManager {
private:
    // 线程池配置
    static const int MAX_THREADS = 50;
    static const int MIN_THREADS = 5;
    
    // 线程管理
    std::vector<std::thread> m_threads;              // 线程池
    std::vector<ThreadWorker*> m_workers;            // 工作线程
    std::atomic<int> m_activeThreads;                // 活跃线程数
    std::atomic<int> m_availableThreads;             // 可用线程数
    
    // 任务队列
    ThreadSafeQueue<ThreadTask> m_taskQueue;         // 任务队列
    std::atomic<bool> m_shutdown;                    // 关闭标志
    
    // 线程调度
    ThreadScheduler* m_scheduler;                    // 线程调度器
    LoadBalancer* m_loadBalancer;                    // 负载均衡器
    
    // 统计信息
    std::atomic<uint64_t> m_totalTasksProcessed;
    std::atomic<uint64_t> m_totalTasksFailed;
    ThreadPoolStatistics m_statistics;
    
public:
    ThreadPoolManager();
    ~ThreadPoolManager();
    
    // 线程池控制
    bool initialize(int initialThreads = MIN_THREADS);
    void shutdown();
    bool isRunning() const;
    
    // 任务提交
    bool submitTask(const ThreadTask& task);
    bool submitTask(std::function<void()> taskFunction, TaskPriority priority = TaskPriority::Normal);
    
    // 线程管理
    bool addThread();
    bool removeThread();
    void adjustThreadCount(int targetCount);
    
    // 状态查询
    int getActiveThreadCount() const;
    int getAvailableThreadCount() const;
    size_t getQueuedTaskCount() const;
    
    // 性能监控
    ThreadPoolStatistics getStatistics() const;
    std::vector<ThreadStatus> getThreadStatuses() const;
    
    // 配置管理
    void setMaxThreads(int maxThreads);
    void setMinThreads(int minThreads);
    void setSchedulingPolicy(SchedulingPolicy policy);
    
private:
    // 工作线程函数
    void workerThreadFunction(int threadId);
    
    // 线程调度
    ThreadWorker* selectBestWorker();
    void balanceThreadLoad();
    
    // 动态调整
    void monitorAndAdjust();
    bool shouldAddThread() const;
    bool shouldRemoveThread() const;
    
    // 清理资源
    void cleanupFinishedThreads();
};
```

### 5. BackgroundUploader - 后台上传器

**职责**：
- 执行单个文件的阻塞上传
- 一个线程处理一个文件
- 阻塞等待上传完成
- 网络协议选择和切换

```cpp
class BackgroundUploader {
private:
    // 上传信息
    std::string m_fileId;
    std::string m_filePath;
    size_t m_fileSize;
    
    // 线程管理
    std::thread m_uploadThread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_completed;
    std::atomic<bool> m_cancelled;
    
    // 网络组件
    NetworkAdapter* m_networkAdapter;
    NetworkProtocolSelector* m_protocolSelector;
    
    // 进度跟踪
    std::atomic<int> m_progress;
    std::atomic<size_t> m_bytesUploaded;
    std::chrono::steady_clock::time_point m_startTime;
    
    // 回调接口
    IUploadProgressCallback* m_progressCallback;
    IUploadCompleteCallback* m_completeCallback;
    
    // 上传配置
    UploadConfig m_config;
    
public:
    BackgroundUploader(const std::string& fileId, 
                      const std::string& filePath,
                      IUploadProgressCallback* progressCallback = nullptr,
                      IUploadCompleteCallback* completeCallback = nullptr);
    ~BackgroundUploader();
    
    // 上传控制
    bool startUpload();
    void cancelUpload();
    bool isRunning() const;
    bool isCompleted() const;
    
    // 进度查询
    int getProgress() const;
    size_t getBytesUploaded() const;
    double getUploadSpeed() const;
    std::chrono::milliseconds getElapsedTime() const;
    
    // 配置管理
    void setUploadConfig(const UploadConfig& config);
    UploadConfig getUploadConfig() const;
    
    // 网络适配器
    void setNetworkAdapter(NetworkAdapter* adapter);
    NetworkAdapter* getNetworkAdapter() const;
    
private:
    // 上传线程主函数 - 阻塞执行
    void uploadThreadFunction();
    
    // 阻塞上传实现
    bool performBlockingUpload();
    
    // 文件分块上传
    bool uploadFileInChunks();
    bool uploadChunk(const void* data, size_t size, size_t chunkIndex);
    
    // 网络协议选择
    bool selectAndInitializeProtocol();
    bool switchProtocol(NetworkProtocol newProtocol);
    
    // 进度更新
    void updateProgress(size_t bytesUploaded);
    void reportProgress();
    
    // 错误处理
    void handleUploadError(const std::string& errorMessage);
    bool retryUpload();
    
    // 完成处理
    void handleUploadComplete(bool success, const std::string& message);
};
```

### 6. NetworkAdapter - 网络适配器

**职责**：
- 封装网络协议实现
- 支持多种传输协议（HTTP、TCP、WebSocket）
- 提供统一的网络接口
- 实现连接池和重连机制

```cpp
class NetworkAdapter {
protected:
    // 连接信息
    std::string m_serverAddress;
    int m_serverPort;
    NetworkProtocol m_protocol;
    
    // 连接状态
    std::atomic<bool> m_connected;
    std::atomic<bool> m_connecting;
    
    // 连接配置
    NetworkConfig m_config;
    
    // 统计信息
    NetworkStatistics m_statistics;
    
public:
    NetworkAdapter(const std::string& serverAddress, int serverPort, NetworkProtocol protocol);
    virtual ~NetworkAdapter();
    
    // 连接管理
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const;
    virtual bool reconnect();
    
    // 数据传输
    virtual bool sendChunk(const void* data, size_t size) = 0;
    virtual bool sendMetadata(const FileMetadata& metadata) = 0;
    virtual bool receiveResponse(ResponseData& response) = 0;
    
    // 配置管理
    void setConfig(const NetworkConfig& config);
    NetworkConfig getConfig() const;
    
    // 统计信息
    NetworkStatistics getStatistics() const;
    void resetStatistics();
    
    // 工厂方法
    static std::unique_ptr<NetworkAdapter> createAdapter(NetworkProtocol protocol, 
                                                         const std::string& serverAddress, 
                                                         int serverPort);
    
protected:
    // 内部方法
    virtual bool establishConnection() = 0;
    virtual void closeConnection() = 0;
    virtual bool validateConnection() = 0;
    
    // 统计更新
    void updateStatistics(size_t bytesSent, bool success);
    void recordConnectionAttempt(bool success);
};

// HTTP适配器实现
class HttpNetworkAdapter : public NetworkAdapter {
private:
    HINTERNET m_session;
    HINTERNET m_connection;
    HINTERNET m_request;
    
    // HTTP特定配置
    std::string m_userAgent;
    std::map<std::string, std::string> m_headers;
    
public:
    HttpNetworkAdapter(const std::string& serverAddress, int serverPort);
    ~HttpNetworkAdapter();
    
    // NetworkAdapter接口实现
    bool connect() override;
    void disconnect() override;
    bool sendChunk(const void* data, size_t size) override;
    bool sendMetadata(const FileMetadata& metadata) override;
    bool receiveResponse(ResponseData& response) override;
    
    // HTTP特定方法
    void addHeader(const std::string& name, const std::string& value);
    void setUserAgent(const std::string& userAgent);
    bool sendPostRequest(const std::string& path, const void* data, size_t size);
    
protected:
    bool establishConnection() override;
    void closeConnection() override;
    bool validateConnection() override;
    
private:
    bool initializeWinHttp();
    void cleanupWinHttp();
    bool sendHttpChunk(const void* data, size_t size);
    std::string buildHttpHeaders() const;
};

// TCP适配器实现
class TcpNetworkAdapter : public NetworkAdapter {
private:
    SOCKET m_socket;
    sockaddr_in m_serverAddr;
    
    // TCP特定配置
    int m_keepAliveInterval;
    int m_sendBufferSize;
    int m_receiveBufferSize;
    
public:
    TcpNetworkAdapter(const std::string& serverAddress, int serverPort);
    ~TcpNetworkAdapter();
    
    // NetworkAdapter接口实现
    bool connect() override;
    void disconnect() override;
    bool sendChunk(const void* data, size_t size) override;
    bool sendMetadata(const FileMetadata& metadata) override;
    bool receiveResponse(ResponseData& response) override;
    
    // TCP特定方法
    void setKeepAlive(int interval);
    void setBufferSizes(int sendBuffer, int receiveBuffer);
    bool setSocketOption(int level, int optname, const void* optval, int optlen);
    
protected:
    bool establishConnection() override;
    void closeConnection() override;
    bool validateConnection() override;
    
private:
    bool initializeSocket();
    void cleanupSocket();
    bool sendTcpData(const void* data, size_t size);
    bool receiveTcpData(void* buffer, size_t size, size_t& received);
};
```

### 7. NetworkProtocolSelector - 网络协议选择器

**职责**：
- 根据网络条件选择最优协议
- 监控网络质量和性能
- 实现协议切换策略
- 提供协议性能评估

```cpp
class NetworkProtocolSelector {
private:
    // 协议配置
    std::vector<NetworkProtocol> m_availableProtocols;
    NetworkProtocol m_currentProtocol;
    NetworkProtocol m_preferredProtocol;
    
    // 性能监控
    std::map<NetworkProtocol, ProtocolPerformance> m_performanceMap;
    NetworkQualityMonitor* m_qualityMonitor;
    
    // 选择策略
    ProtocolSelectionStrategy m_strategy;
    ProtocolSwitchPolicy m_switchPolicy;
    
    // 评估参数
    double m_speedWeight = 0.4;        // 速度权重
    double m_stabilityWeight = 0.3;    // 稳定性权重
    double m_latencyWeight = 0.2;      // 延迟权重
    double m_reliabilityWeight = 0.1;  // 可靠性权重
    
public:
    NetworkProtocolSelector();
    ~NetworkProtocolSelector();
    
    // 协议选择
    NetworkProtocol selectBestProtocol();
    NetworkProtocol selectProtocolForFile(const FileInfo& fileInfo);
    bool shouldSwitchProtocol(NetworkProtocol currentProtocol);
    
    // 协议管理
    void addProtocol(NetworkProtocol protocol);
    void removeProtocol(NetworkProtocol protocol);
    void setPreferredProtocol(NetworkProtocol protocol);
    
    // 性能评估
    void recordProtocolPerformance(NetworkProtocol protocol, const PerformanceMetrics& metrics);
    ProtocolPerformance getProtocolPerformance(NetworkProtocol protocol) const;
    
    // 策略配置
    void setSelectionStrategy(ProtocolSelectionStrategy strategy);
    void setSwitchPolicy(const ProtocolSwitchPolicy& policy);
    void setWeights(double speed, double stability, double latency, double reliability);
    
    // 监控接口
    void startQualityMonitoring();
    void stopQualityMonitoring();
    NetworkQuality getCurrentNetworkQuality() const;
    
private:
    // 协议评估
    double evaluateProtocol(NetworkProtocol protocol) const;
    double calculateProtocolScore(const ProtocolPerformance& performance) const;
    
    // 选择算法
    NetworkProtocol selectByPerformance();
    NetworkProtocol selectByStability();
    NetworkProtocol selectByLatency();
    NetworkProtocol selectByAdaptive();
    
    // 切换判断
    bool shouldSwitchForBetterPerformance(NetworkProtocol current, NetworkProtocol candidate);
    bool shouldSwitchForStability(NetworkProtocol current);
    
    // 网络质量分析
    void analyzeNetworkConditions();
    void updateProtocolRecommendations();
};

// 协议性能数据
struct ProtocolPerformance {
    double averageSpeed;           // 平均速度 (MB/s)
    double averageLatency;         // 平均延迟 (ms)
    double successRate;            // 成功率 (%)
    double connectionTime;         // 连接时间 (ms)
    double stabilityScore;         // 稳定性评分 (0-100)
    int totalAttempts;            // 总尝试次数
    int successfulAttempts;       // 成功次数
    std::chrono::steady_clock::time_point lastUpdate;
    
    ProtocolPerformance();
    void updateMetrics(const PerformanceMetrics& metrics);
    double calculateOverallScore() const;
};
```

### 8. ConfigurationManager - 配置管理器

**职责**：
- 管理服务配置
- 支持热配置更新
- 配置验证和校验
- 配置文件监控

```cpp
class ConfigurationManager {
private:
    // 配置存储
    ServiceConfig m_currentConfig;
    ServiceConfig m_defaultConfig;
    std::string m_configFilePath;
    
    // 配置监控
    std::unique_ptr<FileWatcher> m_fileWatcher;
    std::atomic<bool> m_watchingConfig;
    
    // 配置更新
    std::vector<IConfigurationListener*> m_listeners;
    std::mutex m_listenersMutex;
    
    // 配置验证
    std::unique_ptr<ConfigValidator> m_validator;
    
    // 线程安全
    mutable std::shared_mutex m_configMutex;
    
public:
    ConfigurationManager();
    ~ConfigurationManager();
    
    // 配置加载
    bool loadConfiguration(const std::string& configFile);
    bool reloadConfiguration();
    ServiceConfig getConfiguration() const;
    
    // 配置更新
    bool updateConfiguration(const ServiceConfig& newConfig);
    bool updateConfigValue(const std::string& key, const std::string& value);
    bool resetToDefault();
    
    // 配置保存
    bool saveConfiguration() const;
    bool saveConfiguration(const std::string& configFile) const;
    
    // 配置监控
    bool startConfigWatching();
    void stopConfigWatching();
    bool isWatchingConfig() const;
    
    // 监听器管理
    void addListener(IConfigurationListener* listener);
    void removeListener(IConfigurationListener* listener);
    
    // 配置验证
    bool validateConfiguration(const ServiceConfig& config) const;
    std::vector<std::string> getValidationErrors(const ServiceConfig& config) const;
    
    // 配置查询
    template<typename T>
    T getConfigValue(const std::string& key) const;
    
    template<typename T>
    bool setConfigValue(const std::string& key, const T& value);
    
    // 配置导入导出
    bool exportConfiguration(const std::string& filePath, ConfigFormat format = ConfigFormat::INI) const;
    bool importConfiguration(const std::string& filePath, ConfigFormat format = ConfigFormat::INI);
    
private:
    // 内部方法
    void onConfigFileChanged();
    void notifyListeners(const ServiceConfig& oldConfig, const ServiceConfig& newConfig);
    bool mergeConfiguration(const ServiceConfig& base, const ServiceConfig& update, ServiceConfig& result);
    
    // 配置解析
    bool parseIniFile(const std::string& filePath, ServiceConfig& config);
    bool parseJsonFile(const std::string& filePath, ServiceConfig& config);
    bool parseXmlFile(const std::string& filePath, ServiceConfig& config);
    
    // 配置写入
    bool writeIniFile(const std::string& filePath, const ServiceConfig& config) const;
    bool writeJsonFile(const std::string& filePath, const ServiceConfig& config) const;
    bool writeXmlFile(const std::string& filePath, const ServiceConfig& config) const;
};

// 配置监听器接口
class IConfigurationListener {
public:
    virtual ~IConfigurationListener() = default;
    virtual void onConfigurationChanged(const ServiceConfig& oldConfig, const ServiceConfig& newConfig) = 0;
    virtual void onConfigurationReloaded(const ServiceConfig& config) = 0;
    virtual void onConfigurationError(const std::string& errorMessage) = 0;
};

// 配置验证器
class ConfigValidator {
public:
    struct ValidationRule {
        std::string key;
        std::function<bool(const std::string&)> validator;
        std::string errorMessage;
    };
    
    ConfigValidator();
    
    // 验证方法
    bool validate(const ServiceConfig& config) const;
    std::vector<std::string> getErrors(const ServiceConfig& config) const;
    
    // 规则管理
    void addRule(const ValidationRule& rule);
    void removeRule(const std::string& key);
    void clearRules();
    
private:
    std::vector<ValidationRule> m_rules;
    
    // 内置验证规则
    void setupDefaultRules();
    bool validatePortNumber(const std::string& value) const;
    bool validateFilePath(const std::string& value) const;
    bool validateThreadCount(const std::string& value) const;
    bool validateQueueSize(const std::string& value) const;
};
```

## 核心流程

### 1. 服务启动流程

```cpp
bool LocalUploadService::start() {
    // 1. 加载配置
    if (!loadConfiguration("config.ini")) {
        return false;
    }
    
    // 2. 初始化组件
    if (!initializeComponents()) {
        return false;
    }
    
    // 3. 启动Named Pipe服务器
    if (!m_pipeServer->start()) {
        return false;
    }
    
    // 4. 启动线程池
    if (!m_threadPoolManager->initialize()) {
        return false;
    }
    
    // 5. 启动进度报告器
    if (!m_progressReporter->start()) {
        return false;
    }
    
    // 6. 设置运行状态
    m_running.store(true);
    m_status = ServiceStatus::Running;
    
    // 7. 启动服务主循环
    std::thread serviceThread(&LocalUploadService::serviceMainLoop, this);
    serviceThread.detach();
    
    return true;
}
```

### 2. 上传请求处理流程

```cpp
void LocalUploadService::handleUploadRequest(const UploadNotificationMessage& message) {
    // 1. 解析消息，创建上传任务
    UploadTask task;
    task.fileId = message.fileId;
    task.filePath = message.filePath;
    task.fileSize = message.fileSize;
    task.priority = static_cast<TaskPriority>(message.priority);
    
    // 2. 验证文件
    if (!task.validateFile()) {
        handleUploadComplete(task.fileId, false, "File validation failed");
        return;
    }
    
    // 3. 添加到上传队列
    if (!m_queueManager->enqueue(task)) {
        handleUploadComplete(task.fileId, false, "Queue is full");
        return;
    }
    
    // 4. 尝试立即启动上传
    if (m_threadPoolManager->getAvailableThreadCount() > 0) {
        startBackgroundUpload(task);
    }
}
```

### 3. 后台上传启动流程

```cpp
bool LocalUploadService::startBackgroundUpload(const UploadTask& task) {
    // 1. 创建后台上传器
    auto uploader = std::make_unique<BackgroundUploader>(
        task.fileId, 
        task.filePath,
        m_progressReporter,     // 进度回调
        this                    // 完成回调
    );
    
    // 2. 配置上传器
    UploadConfig config = getDefaultUploadConfig();
    uploader->setUploadConfig(config);
    
    // 3. 提交到线程池
    auto uploadTask = [uploader = uploader.get()]() {
        // 阻塞执行上传 - 按照原始需求
        uploader->startUpload();
    };
    
    if (m_threadPoolManager->submitTask(uploadTask, task.priority)) {
        // 4. 记录活跃上传器
        {
            std::lock_guard<std::mutex> lock(m_uploadersMutex);
            m_activeUploaders[task.fileId] = uploader.release();
        }
        return true;
    }
    
    return false;
}
```

### 4. 阻塞上传实现

```cpp
void BackgroundUploader::uploadThreadFunction() {
    try {
        // 1. 选择网络协议
        if (!selectAndInitializeProtocol()) {
            handleUploadComplete(false, "Failed to initialize network protocol");
            return;
        }
        
        // 2. 执行阻塞上传 - 严格按照原始需求
        bool success = performBlockingUpload();
        
        // 3. 处理上传结果
        handleUploadComplete(success, success ? "Upload completed" : "Upload failed");
        
    } catch (const std::exception& e) {
        handleUploadError(e.what());
    }
}

bool BackgroundUploader::performBlockingUpload() {
    // 打开文件
    std::ifstream file(m_filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // 连接服务器
    if (!m_networkAdapter->connect()) {
        return false;
    }
    
    // 阻塞上传文件内容
    const size_t chunkSize = 64 * 1024;  // 64KB chunks
    std::vector<char> buffer(chunkSize);
    size_t totalUploaded = 0;
    
    while (!file.eof() && !m_cancelled.load()) {
        // 读取数据块
        file.read(buffer.data(), chunkSize);
        size_t bytesRead = file.gcount();
        
        if (bytesRead > 0) {
            // 阻塞发送数据块 - 等待发送成功后才继续下一步
            if (!m_networkAdapter->sendChunk(buffer.data(), bytesRead)) {
                return false;
            }
            
            // 更新进度
            totalUploaded += bytesRead;
            updateProgress(totalUploaded);
        }
    }
    
    // 断开连接
    m_networkAdapter->disconnect();
    
    return !m_cancelled.load() && (totalUploaded == m_fileSize);
}
```

## 进程间通信协议

### 1. 消息格式定义

```cpp
// 上传通知消息
struct UploadNotificationMessage {
    MessageHeader header;           // 消息头
    char fileId[64];               // 文件ID
    char filePath[512];            // 文件路径
    uint64_t fileSize;             // 文件大小
    char fileHash[33];             // 文件哈希
    uint32_t priority;             // 优先级
    uint64_t timestamp;            // 时间戳
    char metadata[256];            // 扩展元数据
};

// 上传完成消息
struct UploadCompleteMessage {
    MessageHeader header;
    char fileId[64];
    uint32_t success;              // 成功标志
    char message[256];             // 结果消息
    uint64_t bytesUploaded;        // 已上传字节数
    uint32_t errorCode;            // 错误代码
};

// 进度更新消息
struct ProgressUpdateMessage {
    MessageHeader header;
    char fileId[64];
    uint32_t progress;             // 进度百分比
    uint64_t bytesUploaded;        // 已上传字节数
    uint32_t speed;                // 上传速度 (KB/s)
    char status[128];              // 状态描述
};
```

### 2. 消息处理器

```cpp
class MessageProcessor {
public:
    virtual ~MessageProcessor() = default;
    
    // 处理上传通知
    virtual void processUploadNotification(const UploadNotificationMessage& message) = 0;
    
    // 处理控制消息
    virtual void processControlMessage(const ControlMessage& message) = 0;
    
    // 处理心跳消息
    virtual void processHeartbeat(const HeartbeatMessage& message) = 0;
    
    // 发送响应
    virtual ResponseMessage createResponse(const MessageHeader& request, 
                                         ResponseCode code, 
                                         const std::string& message) = 0;
};
```

## 进度报告机制

### 1. ProgressReporter - 进度报告器

```cpp
class ProgressReporter {
private:
    // 报告配置
    std::chrono::milliseconds m_reportInterval;     // 报告间隔（1秒）
    bool m_enableReporting;
    
    // 线程管理
    std::thread m_reportThread;
    std::atomic<bool> m_running;
    
    // 进度数据
    std::map<std::string, ProgressData> m_progressMap;
    std::mutex m_progressMutex;
    
    // 回调接口
    std::vector<IProgressListener*> m_listeners;
    
public:
    ProgressReporter(std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
    ~ProgressReporter();
    
    // 控制方法
    bool start();
    void stop();
    
    // 进度更新
    void updateProgress(const std::string& fileId, int progress, size_t bytesUploaded);
    void removeProgress(const std::string& fileId);
    
    // 监听器管理
    void addListener(IProgressListener* listener);
    void removeListener(IProgressListener* listener);
    
    // 配置
    void setReportInterval(std::chrono::milliseconds interval);
    
private:
    // 报告线程主函数
    void reportThreadFunction();
    
    // 发送进度报告 - 每秒调用
    void sendProgressReports();
    
    // 通知监听器
    void notifyListeners(const std::string& fileId, const ProgressData& data);
};

// 进度数据结构
struct ProgressData {
    int progress;                  // 进度百分比
    size_t bytesUploaded;         // 已上传字节数
    size_t totalBytes;            // 总字节数
    std::chrono::steady_clock::time_point lastUpdate;  // 最后更新时间
    double speed;                 // 上传速度 (MB/s)
    
    ProgressData() : progress(0), bytesUploaded(0), totalBytes(0), speed(0.0) {}
};
```

## 配置管理

### 1. 服务配置

```cpp
struct ServiceConfig {
    // 基本配置
    std::string serviceName = "HighPerformanceUploadService";
    std::string pipeName = "\\\\.\\pipe\\UploadService";
    int maxConnections = 10;
    
    // 线程池配置
    int minThreads = 5;
    int maxThreads = 50;
    int threadIdleTimeout = 30000;  // 毫秒
    
    // 队列配置
    size_t maxQueueSize = 1000;
    bool enableQueuePersistence = true;
    std::string queuePersistenceFile = "upload_queue.dat";
    
    // 进度报告配置
    int progressReportInterval = 1000;  // 毫秒
    bool enableProgressReport = true;
    
    // 网络配置
    std::string serverAddress = "localhost";
    int serverPort = 8888;
    int connectionTimeout = 30000;
    int readTimeout = 30000;
    
    // 上传配置
    size_t chunkSize = 64 * 1024;  // 64KB
    int maxRetries = 3;
    int retryDelay = 1000;  // 毫秒
    
    // 日志配置
    std::string logLevel = "INFO";
    std::string logFile = "upload_service.log";
    bool enableConsoleLog = true;
};
```

### 2. 配置加载器

```cpp
class ConfigLoader {
public:
    static bool loadFromFile(const std::string& configFile, ServiceConfig& config);
    static bool saveToFile(const std::string& configFile, const ServiceConfig& config);
    static ServiceConfig getDefaultConfig();
    
private:
    static bool parseConfigLine(const std::string& line, std::string& key, std::string& value);
    static void setConfigValue(ServiceConfig& config, const std::string& key, const std::string& value);
};
```

## 监控和统计

### 1. 服务统计

```cpp
struct ServiceStatistics {
    // 基本统计
    uint64_t totalProcessed;       // 总处理数量
    uint64_t totalCompleted;       // 总完成数量
    uint64_t totalFailed;          // 总失败数量
    uint64_t totalBytes;           // 总传输字节数
    
    // 性能指标
    double averageSpeed;           // 平均速度 (MB/s)
    std::chrono::milliseconds averageTime;  // 平均完成时间
    double successRate;            // 成功率
    
    // 资源使用
    int activeThreads;             // 活跃线程数
    size_t queueSize;             // 队列大小
    double cpuUsage;              // CPU使用率
    size_t memoryUsage;           // 内存使用量
    
    // 时间统计
    std::chrono::steady_clock::time_point startTime;
    std::chrono::milliseconds uptime;
    
    ServiceStatistics();
    std::string generateReport() const;
    std::string toJson() const;
};
```

## 错误处理和日志

### 1. 错误处理策略

```cpp
class ErrorHandler {
public:
    static void handleCriticalError(const std::string& component, const std::string& error);
    static void handleRecoverableError(const std::string& component, const std::string& error);
    static void handleWarning(const std::string& component, const std::string& warning);
    
    static bool shouldRetry(ErrorCode errorCode);
    static std::chrono::milliseconds getRetryDelay(int attemptCount);
    
private:
    static void notifyAdministrator(const std::string& message);
    static void restartComponent(const std::string& component);
};
```

### 2. 日志管理

```cpp
class ServiceLogger {
public:
    enum class LogLevel { DEBUG, INFO, WARNING, ERROR, CRITICAL };
    
    static void log(LogLevel level, const std::string& component, const std::string& message);
    static void setLogLevel(LogLevel level);
    static void enableFileLogging(const std::string& logFile);
    static void enableConsoleLogging(bool enable);
    
private:
    static std::string formatLogMessage(LogLevel level, const std::string& component, const std::string& message);
    static void writeToFile(const std::string& message);
    static void writeToConsole(const std::string& message);
};
```

## 使用示例

### 1. 服务启动

```cpp
int main() {
    // 创建服务实例
    LocalUploadService service;
    
    // 加载配置
    if (!service.loadConfiguration("config.ini")) {
        std::cerr << "Failed to load configuration" << std::endl;
        return 1;
    }
    
    // 启动服务
    if (!service.start()) {
        std::cerr << "Failed to start service" << std::endl;
        return 1;
    }
    
    std::cout << "Upload service started successfully" << std::endl;
    
    // 等待停止信号
    std::cin.get();
    
    // 停止服务
    service.stop();
    
    return 0;
}
```

### 2. 监控服务状态

```cpp
void monitorService(LocalUploadService& service) {
    while (service.getStatus() == ServiceStatus::Running) {
        auto stats = service.getStatistics();
        
        std::cout << "活跃线程: " << stats.activeThreads << std::endl;
        std::cout << "队列大小: " << stats.queueSize << std::endl;
        std::cout << "成功率: " << stats.successRate << "%" << std::endl;
        std::cout << "平均速度: " << stats.averageSpeed << " MB/s" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}
```

## 性能优化

- **预分配资源**：预先创建线程和内存池
- **批量处理**：批量处理消息和任务
- **缓存优化**：文件读取缓存
- **网络优化**：连接池和Keep-Alive
- **内存管理**：对象池和智能指针

## 注意事项

1. **进程隔离**：作为独立进程运行，确保稳定性
2. **资源管理**：严格控制线程和内存使用
3. **错误恢复**：完善的错误处理和恢复机制
4. **性能监控**：实时监控服务状态和性能
5. **配置管理**：支持动态配置更新

---

*文档版本：v1.0*  
*更新时间：2025年6月21日*
