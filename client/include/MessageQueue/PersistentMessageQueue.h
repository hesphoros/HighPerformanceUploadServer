#ifndef PRESISTENT_MESSAGE_QUEUE_H
#define PRESISTENT_MESSAGE_QUEUE_H

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>



/**
 * @brief 消息结构体
 */
struct IpcMessage
{
    uint64_t                id;                    // 消息唯一ID
    uint64_t                timestamp;             // 时间戳(ms)
    uint32_t                priority;              // 优先级(0最高)
    std::vector<uint8_t>    data;                  // 消息数据

    IpcMessage()
        : id(0), timestamp(0), priority(0) {
    }

    IpcMessage(uint64_t msg_id, const std::vector<uint8_t>& msg_data, uint32_t msg_priority = 0)
        : id(msg_id)
        , timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count())
        , priority(msg_priority)
        , data(msg_data) {
    }
};

/**
 * @brief 消息信息结构体（用于日志输出）
 */
struct MessageInfo
{
    uint64_t id;
    uint64_t timestamp;
    uint32_t priority;
    uint32_t data_size;
};

/**
 * @brief 高性能无锁持久化消息队列
 *
 * 特性:
 * 1. 无锁内存队列(使用环形缓冲区)
 * 2. 内存满后溢出到磁盘
 * 3. 按优先级恢复消息
 * 4. 支持持久化和恢复
 */
class PersistentMessageQueue
{
public:
    /**
     * @brief 构造函数
     * @param persist_dir    持久化目录
     * @param memory_capacity 内存队列容量(条数)
     * @param max_disk_size   最大磁盘占用(字节)
     */
    explicit PersistentMessageQueue(const std::filesystem::path& persist_dir,
        size_t memory_capacity = 1024,
        size_t max_disk_size = 100 * 1024 * 1024);

    ~PersistentMessageQueue();

    // 禁止拷贝
    PersistentMessageQueue(const PersistentMessageQueue&) = delete;
    PersistentMessageQueue& operator=(const PersistentMessageQueue&) = delete;

    /**
     * @brief 入队(无锁，生产者调用)
     * @param message 消息
     * @return 是否成功(false表示队列已满)
     */
    bool enqueue(IpcMessage&& message);

    /**
     * @brief  出队(无锁，消费者调用)
     * @return 消息(如果队列为空返回nullopt)
     */
    std::optional<IpcMessage> dequeue();

    /**
     * @brief 查看队首消息但不删除(无锁，用于发送失败重试)
     * @return 消息(如果队列为空返回nullopt)
     */
    std::optional<IpcMessage> peek() const;

    /**
     * @brief 删除队首消息(发送成功后调用)
     * @return 是否成功
     */
    bool pop_front();

    /**
     * @brief 获取队列大小(近似值)
     */
    size_t size() const;

    /**
     * @brief 是否为空
     */
    bool empty() const;

    /**
     * @brief 清空队列
     */
    void clear();

    /**
     * @brief 从磁盘加载消息到内存
     * @return 加载的消息数
     */
    size_t load_from_disk();

    /**
     * @brief 刷新内存队列到磁盘
     * @return 刷新的消息数
     */
    size_t flush_to_disk();

    /**
     * @brief 获取统计信息
     */
    struct Statistics
    {
        size_t memory_size;       // 内存队列大小
        size_t disk_size;         // 磁盘队列大小
        size_t total_enqueued;    // 总入队数
        size_t total_dequeued;    // 总出队数
        size_t disk_bytes;        // 磁盘占用字节
    };
    Statistics get_statistics() const;

private:
    // 内存队列节点
    struct MemoryNode
    {
        IpcMessage          message;
        std::atomic<bool>   ready{ false }; // 是否已写入完成
    };

    // 环形缓冲区索引
    size_t next_index(size_t idx) const { return (idx + 1) % memory_capacity_; }

    // 磁盘操作
    bool                        write_to_disk(const IpcMessage& message);
    std::optional<IpcMessage>   read_from_disk();
    void                        rebuild_disk_index();

    // 序列化
    static std::vector<uint8_t>         serialize_message(const IpcMessage& message);
    static std::optional<IpcMessage>    deserialize_message(const std::vector<uint8_t>& data);

private:
    // 内存队列
    std::unique_ptr<MemoryNode[]>                   memory_buffer_;          ///< 内存队列
    size_t                                          memory_capacity_;        ///< 内存队列容量
    alignas(64) std::atomic<size_t>                 write_pos_{ 0 };         ///< 写入位置索引
    alignas(64) std::atomic<size_t>                 read_pos_{ 0 };          ///< 读取位置索引

    // 磁盘队列
    std::filesystem::path                           persist_dir_;            ///< 持久化目录
    std::filesystem::path                           data_file_path_;         ///< 数据文件路径
    std::filesystem::path                           index_file_path_;        ///< 索引文件路径
    std::ofstream                                   data_writer_;            ///< 数据写入流
    std::ifstream                                   data_reader_;            ///<  数据读取流
    size_t                                          max_disk_size_;          ///< 最大磁盘占用(字节)
    std::atomic<size_t>                             current_disk_size_{ 0 }; ///< 当前磁盘占用大小

    // 磁盘索引(offset -> size)
    mutable std::mutex                              disk_mutex_;             ///< 只保护磁盘操作
    std::vector<std::pair<uint64_t, uint32_t>>      disk_index_;             ///< (offset, size)
    size_t                                          disk_read_pos_{ 0 };     ///< 读取位置
    std::vector<MessageInfo>                        loaded_messages_info_;   ///< 加载的消息信息(用于日志)

    // 统计
    std::atomic<uint64_t>                           total_enqueued_{ 0 };    ///< 总入队数
    std::atomic<uint64_t>                           total_dequeued_{ 0 };    ///< 总出队数
    std::atomic<uint64_t>                           next_message_id_{ 1 };   ///< 下一个消息ID
};


#endif // PRESISTENT_MESSAGE_QUEUE_H