#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief 行级锁队列实现 - 核心数据结构
 * 
 * 设计要点：
 * - enqueueMutex: 专门用于入队操作
 * - dequeueMutex: 专门用于出队操作  
 * - 条件变量只在有数据时唤醒
 * - atomic计数器避免锁竞争
 */
template<typename T>
class ThreadSafeRowLockQueue {
private:
    mutable std::mutex mEnqueueMutex;    // 入队专用锁
    mutable std::mutex mDequeueMutex;    // 出队专用锁
    std::queue<T> mDataQueue;            // 数据队列
    std::condition_variable mWorkCV;     // 条件变量
    std::atomic<size_t> mSize{0};        // 原子计数器

public:
    // UI线程调用：入队（只锁入队操作）
    void push(const T& item) {
        {
            std::unique_lock<std::mutex> lock(mEnqueueMutex);
            mDataQueue.push(item);
            mSize.fetch_add(1);
        }
        mWorkCV.notify_one();  // 唤醒通知线程
    }
    
    void push(T&& item) {
        {
            std::unique_lock<std::mutex> lock(mEnqueueMutex);
            mDataQueue.push(std::move(item));
            mSize.fetch_add(1);
        }
        mWorkCV.notify_one();
    }
    
    // 通知线程调用：出队（只锁出队操作）
    bool waitAndPop(T& item) {
        std::unique_lock<std::mutex> lock(mDequeueMutex);
        
        // 等待数据可用
        mWorkCV.wait(lock, [this] { return mSize.load() > 0; });
        
        if (!mDataQueue.empty()) {
            item = std::move(mDataQueue.front());
            mDataQueue.pop();
            mSize.fetch_sub(1);
            return true;
        }
        return false;
    }
    
    // 非阻塞尝试出队
    bool tryPop(T& item) {
        std::unique_lock<std::mutex> lock(mDequeueMutex);
        
        if (!mDataQueue.empty()) {
            item = std::move(mDataQueue.front());
            mDataQueue.pop();
            mSize.fetch_sub(1);
            return true;
        }
        return false;
    }
    
    // 查询操作（无锁）
    size_t size() const {
        return mSize.load();
    }
    
    bool empty() const {
        return mSize.load() == 0;
    }
    
    void clear() {
        std::unique_lock<std::mutex> lock1(mEnqueueMutex);
        std::unique_lock<std::mutex> lock2(mDequeueMutex);
        
        std::queue<T> empty;
        mDataQueue.swap(empty);
        mSize.store(0);
    }
};