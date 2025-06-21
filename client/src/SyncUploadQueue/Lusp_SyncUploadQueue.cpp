#include "SyncUploadQueue/Lusp_SyncUploadQueue.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif



class Lusp_SyncUploadQueue::Impl {
public:
    Impl() : autoStart(true), isRunning(false), shouldStop(false) {
        startNotificationThread();
    }
    
    ~Impl() {
        cleanup();
    }
    
    void startNotificationThread() {
        isRunning = true;
        shouldStop = false;
        
        // 启动通知线程
        notificationThread = std::thread([this]() {
            this->notificationThreadLoop();
        });
        
        std::cout << "Lusp_SyncUploadQueue: 通知线程已启动" << std::endl;
    }
    
    void cleanup() {
        shouldStop = true;
        
        // 唤醒通知线程
        uploadQueue.push(Lusp_SyncUploadFileInfo{});  // 发送停止信号
        
        if (notificationThread.joinable()) {
            notificationThread.join();
        }
        
        isRunning = false;
        std::cout << "Lusp_SyncUploadQueue: 通知线程已停止" << std::endl;
    }
    
    void pushFile(const std::string& filePath) {
        Lusp_SyncUploadFileInfo fileInfo;
        fileInfo.sFileFullNameValue = filePath;
        fileInfo.sOnlyFileNameValue = extractFileName(filePath);
        fileInfo.sSyncFileSizeValue = getFileSize(filePath);
        fileInfo.eUploadStatusInf = Lusp_UploadStatusInf::LUSP_UPLOAD_STATUS_IDENTIFIERS_PENDING;
        fileInfo.uUploadTimeStamp = getCurrentTimestamp();
        fileInfo.sLanClientDevice = getComputerName();
        fileInfo.eUploadFileTyped = detectFileType(filePath);
        
       
        {
            std::unique_lock<std::mutex> lock(workMutex);
            uploadQueue.push(std::move(fileInfo));
        }
        
        std::cout << "Lusp_SyncUploadQueue: 文件已入队: " << filePath << std::endl;
    }
    
    void pushFiles(const std::vector<std::string>& filePaths) {
        for (const auto& filePath : filePaths) {
            pushFile(filePath);
        }
    }
    
    void pushFileInfo(const Lusp_SyncUploadFileInfo& fileInfo) {
      
        {
            std::unique_lock<std::mutex> lock(workMutex);
            uploadQueue.push(fileInfo);
        }
        
        std::cout << "Lusp_SyncUploadQueue: 文件信息已入队: " << fileInfo.sFileFullNameValue << std::endl;
    }

private:
    void notificationThreadLoop() {
        std::cout << "Lusp_SyncUploadQueue: 通知线程开始工作" << std::endl;
        
        while (!shouldStop) {
            Lusp_SyncUploadFileInfo fileInfo;
            
            //  行级锁出队 - 条件变量等待
            if (uploadQueue.waitAndPop(fileInfo)) {
                // 检查是否为停止信号
                if (shouldStop || fileInfo.sFileFullNameValue.empty()) {
                    break;
                }
                
                // 给本地服务发通知
                sendNotificationToLocalService(fileInfo);
                
                // 模拟进度回调
                simulateUploadProgress(fileInfo);
            }
        }
        
        std::cout << "Lusp_SyncUploadQueue: 通知线程结束工作" << std::endl;
    }
    
    void sendNotificationToLocalService(const Lusp_SyncUploadFileInfo& fileInfo) {
        // TODO: 实现真实的本地服务通信
        // 这里应该通过Named Pipe或其他IPC方式通知本地服务
        
        std::cout << "📞 通知本地服务上传文件: " << fileInfo.sFileFullNameValue 
                  << " (大小: " << fileInfo.sSyncFileSizeValue << " 字节)" << std::endl;
    }
    
    void simulateUploadProgress(const Lusp_SyncUploadFileInfo& fileInfo) {
        if (!progressCallback) return;
        
        // 模拟上传进度
        for (int progress = 0; progress <= 100; progress += 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            std::string status = "上传中 " + std::to_string(progress) + "%";
            progressCallback(fileInfo.sFileFullNameValue, progress, status);
            
            if (shouldStop) break;
        }
        
        // 完成回调
        if (completedCallback) {
            completedCallback(fileInfo.sFileFullNameValue, true, "上传完成");
        }
    }
    
    // 辅助函数
    std::string extractFileName(const std::string& fullPath) {
        size_t pos = fullPath.find_last_of("/\\");
        return (pos != std::string::npos) ? fullPath.substr(pos + 1) : fullPath;
    }
    
    size_t getFileSize(const std::string& filePath) {
        // TODO: 实现真实的文件大小获取
        return 1024 * 1024;  // 临时返回1MB
    }
    
    uint64_t getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    std::string getComputerName() {
        char buffer[256];
        DWORD size = sizeof(buffer);
        if (GetComputerNameA(buffer, &size)) {
            return std::string(buffer);
        }
        return "Unknown-ComputerName"; // 获取计算机失败返回默认值
    }
    
    Lusp_UploadFileTyped detectFileType(const std::string& filePath) {
        std::string ext = filePath.substr(filePath.find_last_of('.') + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == "jpg" || ext == "png" || ext == "gif" || ext == "bmp") {
            return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_IMAGE;
        } else if (ext == "mp4" || ext == "avi" || ext == "mkv") {
            return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_VIDEO;
        } else if (ext == "txt" || ext == "doc" || ext == "pdf") {
            return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_DOCUMENT;
        }
        return Lusp_UploadFileTyped::LUSP_UPLOADTYPE_UNDEFINED;
    }

public:
    ThreadSafeRowLockQueue<Lusp_SyncUploadFileInfo> uploadQueue;  // 行级锁队列
    std::thread notificationThread;                               // 通知线程
    std::mutex workMutex;                                        // 工作锁
    
    ProgressCallback progressCallback;
    CompletedCallback completedCallback;
    
    std::atomic<bool> autoStart;
    std::atomic<bool> isRunning;
    std::atomic<bool> shouldStop;
};

Lusp_SyncUploadQueue& Lusp_SyncUploadQueue::instance() {
    static Lusp_SyncUploadQueue instance;
    return instance;
}

Lusp_SyncUploadQueue::Lusp_SyncUploadQueue() : d(std::make_unique<Impl>()) {
    std::cout << "Lusp_SyncUploadQueue: 全局实例已创建" << std::endl;
}

Lusp_SyncUploadQueue::~Lusp_SyncUploadQueue() {
    std::cout << "Lusp_SyncUploadQueue: 全局实例正在销毁" << std::endl;
}

void Lusp_SyncUploadQueue::push(const std::string& filePath) {
    d->pushFile(filePath);
}

void Lusp_SyncUploadQueue::push(const std::vector<std::string>& filePaths) {
    d->pushFiles(filePaths);
}

void Lusp_SyncUploadQueue::push(const Lusp_SyncUploadFileInfo& fileInfo) {
    d->pushFileInfo(fileInfo);
}

void Lusp_SyncUploadQueue::setProgressCallback(ProgressCallback callback) {
    d->progressCallback = std::move(callback);
}

void Lusp_SyncUploadQueue::setCompletedCallback(CompletedCallback callback) {
    d->completedCallback = std::move(callback);
}

void Lusp_SyncUploadQueue::setAutoStart(bool autoStart) {
    d->autoStart = autoStart;
}

size_t Lusp_SyncUploadQueue::pendingCount() const {
    return d->uploadQueue.size();
}

bool Lusp_SyncUploadQueue::isActive() const {
    return d->isRunning.load();
}

bool Lusp_SyncUploadQueue::empty() const {
    return d->uploadQueue.empty();
}