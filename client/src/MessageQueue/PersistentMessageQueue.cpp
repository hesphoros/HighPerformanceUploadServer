#include "MessageQueue/PersistentMessageQueue.h"
#include "log_headers.h"
#include "utils/SystemErrorUtil.h"
#include "UniConv.h"
#include <algorithm>
#include <chrono>
#include <cstring>


PersistentMessageQueue::PersistentMessageQueue(const std::filesystem::path& persist_dir, size_t memory_capacity, size_t max_disk_size)
    : memory_capacity_(memory_capacity)
    , persist_dir_(persist_dir)
    , max_disk_size_(max_disk_size) {
    // 初始化内存缓冲区
    memory_buffer_ = std::make_unique<MemoryNode[]>(memory_capacity_);

    // 创建持久化目录
    try {
        if (!std::filesystem::exists(persist_dir_)) {
            std::filesystem::create_directories(persist_dir_);
        }

        data_file_path_ = persist_dir_ / "messages.dat";
        index_file_path_ = persist_dir_ / "messages.idx";
        
        g_LogMessageQueue.WriteLogContent(LOG_INFO,
            "Initializing PersistentMessageQueue with memory capacity " + std::to_string(memory_capacity_) +
            " and max disk size " + std::to_string(max_disk_size_) + " bytes at " + persist_dir_.string());
        g_LogMessageQueue.WriteLogContent(LOG_INFO,
            "Data file path: " + data_file_path_.string());
        g_LogMessageQueue.WriteLogContent(LOG_INFO,
            "Index file path: " + index_file_path_.string());

        // 加载已有数据
        if (std::filesystem::exists(data_file_path_)) {
            rebuild_disk_index();
            size_t loaded_count = disk_index_.size();
            g_LogMessageQueue.WriteLogContent(LOG_INFO,
                "PersistentMessageQueue initialized, loaded " + std::to_string(loaded_count) + " messages from disk");

            // 打印每条加载的消息详情
            if (loaded_count > 0) {
                g_LogMessageQueue.WriteLogContent(LOG_INFO, "=== Loaded Messages Details ===");
                for (size_t i = 0; i < loaded_messages_info_.size(); ++i) {
                    const auto& info = loaded_messages_info_[i];
                    g_LogMessageQueue.WriteLogContent(LOG_INFO,
                        "[" + std::to_string(i + 1) + "/" + std::to_string(loaded_count) + "] " +
                        "ID=" + std::to_string(info.id) +
                        ", Priority=" + std::to_string(info.priority) +
                        ", Size=" + std::to_string(info.data_size) + " bytes" +
                        ", Timestamp=" + std::to_string(info.timestamp));
                }
                g_LogMessageQueue.WriteLogContent(LOG_INFO, "=== End of Loaded Messages ===");
            }
        }


    }
    catch (const std::exception& e) {
        std::string error_msg = UniConv::GetInstance()->ToUtf8FromLocale(e.what());
        g_LogMessageQueue.WriteLogContent(LOG_ERROR,
            "Failed to initialize PersistentMessageQueue: " + error_msg);
    }
}

PersistentMessageQueue::~PersistentMessageQueue() {
    // 刷新剩余消息到磁盘
    flush_to_disk();

    if (data_writer_.is_open())
        data_writer_.close();
    if (data_reader_.is_open())
        data_reader_.close();
}

bool PersistentMessageQueue::enqueue(IpcMessage&& message) {
    // 分配消息ID
    if (message.id == 0) {
        message.id = next_message_id_.fetch_add(1, std::memory_order_relaxed);
    }

    size_t write_idx = write_pos_.load(std::memory_order_relaxed);
    size_t read_idx = read_pos_.load(std::memory_order_acquire);

    // 检查是否满(留一个空位避免读写指针重叠)
    if (next_index(write_idx) == read_idx) {
        // 内存队列满，尝试写入磁盘
        if (write_to_disk(message)) {
            total_enqueued_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        return false; // 磁盘也满
    }

    // 写入内存队列
    memory_buffer_[write_idx].message = std::move(message);
    memory_buffer_[write_idx].ready.store(true, std::memory_order_release);

    // 更新写指针
    write_pos_.store(next_index(write_idx), std::memory_order_release);
    total_enqueued_.fetch_add(1, std::memory_order_relaxed);

    return true;
}

std::optional<IpcMessage> PersistentMessageQueue::dequeue() {
    size_t read_idx = read_pos_.load(std::memory_order_relaxed);
    size_t write_idx = write_pos_.load(std::memory_order_acquire);

    // 内存队列为空
    if (read_idx == write_idx) {
        // 尝试从磁盘读取
        auto disk_msg = read_from_disk();
        if (disk_msg.has_value()) {
            total_dequeued_.fetch_add(1, std::memory_order_relaxed);
        }
        return disk_msg;
    }

    // 等待消息写入完成
    while (!memory_buffer_[read_idx].ready.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // 读取消息
    IpcMessage message = std::move(memory_buffer_[read_idx].message);
    memory_buffer_[read_idx].ready.store(false, std::memory_order_release);

    // 更新读指针
    read_pos_.store(next_index(read_idx), std::memory_order_release);
    total_dequeued_.fetch_add(1, std::memory_order_relaxed);

    return message;
}

size_t PersistentMessageQueue::size() const {
    size_t write_idx = write_pos_.load(std::memory_order_acquire);
    size_t read_idx = read_pos_.load(std::memory_order_acquire);

    size_t memory_size = 0;
    if (write_idx >= read_idx) {
        memory_size = write_idx - read_idx;
    }
    else {
        memory_size = memory_capacity_ - read_idx + write_idx;
    }

    std::lock_guard<std::mutex> lock(disk_mutex_);
    return memory_size + (disk_index_.size() - disk_read_pos_);
}

bool PersistentMessageQueue::empty() const {
    return size() == 0;
}

void PersistentMessageQueue::clear() {
    // 清空内存队列
    write_pos_.store(0, std::memory_order_release);
    read_pos_.store(0, std::memory_order_release);

    // 清空磁盘队列
    std::lock_guard<std::mutex> lock(disk_mutex_);
    disk_index_.clear();
    disk_read_pos_ = 0;
    current_disk_size_.store(0, std::memory_order_release);

    // 删除磁盘文件
    try {
        if (data_writer_.is_open())
            data_writer_.close();
        if (data_reader_.is_open())
            data_reader_.close();

        std::filesystem::remove(data_file_path_);
        std::filesystem::remove(index_file_path_);
    }
    catch (const std::exception& e) {
        std::string error_msg = UniConv::GetInstance()->ToUtf8FromLocale(e.what());
        g_LogMessageQueue.WriteLogContent(LOG_ERROR,
            "Failed to clear disk files: " + error_msg);
    }
}

size_t PersistentMessageQueue::load_from_disk() {
    std::lock_guard<std::mutex> lock(disk_mutex_);

    size_t loaded = 0;
    while (disk_read_pos_ < disk_index_.size()) {
        // 检查内存队列是否有空间
        size_t write_idx = write_pos_.load(std::memory_order_relaxed);
        size_t read_idx = read_pos_.load(std::memory_order_acquire);
        if (next_index(write_idx) == read_idx) {
            break; // 内存队列满
        }

        // 从磁盘读取
        auto& [offset, size] = disk_index_[disk_read_pos_];
        try {
            if (!data_reader_.is_open()) {
                data_reader_.open(data_file_path_, std::ios::binary);
            }

            data_reader_.seekg(offset);
            std::vector<uint8_t> buffer(size);
            data_reader_.read(reinterpret_cast<char*>(buffer.data()), size);

            auto message = deserialize_message(buffer);
            if (!message.has_value()) {
                g_LogMessageQueue.WriteLogContent(LOG_ERROR,
                    "Failed to deserialize message at offset " + std::to_string(offset));
                ++disk_read_pos_;
                continue;
            }                // 写入内存队列(不经过enqueue避免重复ID分配)
            memory_buffer_[write_idx].message = std::move(message.value());
            memory_buffer_[write_idx].ready.store(true, std::memory_order_release);
            write_pos_.store(next_index(write_idx), std::memory_order_release);

            ++disk_read_pos_;
            ++loaded;
        }
        catch (const std::exception& e) {
            std::string error_msg = UniConv::GetInstance()->ToUtf8FromLocale(e.what());
            g_LogMessageQueue.WriteLogContent(LOG_ERROR,
                "Failed to load message from disk: " + error_msg);
            break;
        }
    }

    return loaded;
}    size_t PersistentMessageQueue::flush_to_disk() {
    size_t flushed = 0;

    while (true) {
        size_t read_idx = read_pos_.load(std::memory_order_relaxed);
        size_t write_idx = write_pos_.load(std::memory_order_acquire);

        if (read_idx == write_idx) {
            break; // 内存队列为空
        }

        // 等待消息写入完成
        while (!memory_buffer_[read_idx].ready.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // 写入磁盘
        if (write_to_disk(memory_buffer_[read_idx].message)) {
            memory_buffer_[read_idx].ready.store(false, std::memory_order_release);
            read_pos_.store(next_index(read_idx), std::memory_order_release);
            ++flushed;
        }
        else {
            break; // 磁盘满或写入失败
        }
    }

    return flushed;
}

PersistentMessageQueue::Statistics PersistentMessageQueue::get_statistics() const {
    Statistics stats;

    size_t write_idx = write_pos_.load(std::memory_order_acquire);
    size_t read_idx = read_pos_.load(std::memory_order_acquire);

    if (write_idx >= read_idx) {
        stats.memory_size = write_idx - read_idx;
    }
    else {
        stats.memory_size = memory_capacity_ - read_idx + write_idx;
    }

    std::lock_guard<std::mutex> lock(disk_mutex_);
    stats.disk_size = disk_index_.size() - disk_read_pos_;
    stats.disk_bytes = current_disk_size_.load(std::memory_order_acquire);

    stats.total_enqueued = total_enqueued_.load(std::memory_order_acquire);
    stats.total_dequeued = total_dequeued_.load(std::memory_order_acquire);

    return stats;
}

bool PersistentMessageQueue::write_to_disk(const IpcMessage& message) {
    std::lock_guard<std::mutex> lock(disk_mutex_);

    // 检查磁盘容量
    if (current_disk_size_.load(std::memory_order_relaxed) >= max_disk_size_) {
        g_LogMessageQueue.WriteLogContent(LOG_WARN,
            "Disk queue full, cannot write message " + std::to_string(message.id));
        return false;
    }

    try {
        // 序列化消息
        auto data = serialize_message(message);

        // 打开文件(追加模式)
        if (!data_writer_.is_open()) {
            data_writer_.open(data_file_path_, std::ios::binary | std::ios::app);
        }

        // 获取当前偏移
        uint64_t offset = data_writer_.tellp();

        // 写入数据
        data_writer_.write(reinterpret_cast<const char*>(data.data()), data.size());
        data_writer_.flush();

        // 更新索引
        disk_index_.emplace_back(offset, static_cast<uint32_t>(data.size()));
        current_disk_size_.fetch_add(data.size(), std::memory_order_relaxed);

        return true;
    }
    catch (const std::exception& e) {
        std::string error_msg = UniConv::GetInstance()->ToUtf8FromLocale(e.what());
        g_LogMessageQueue.WriteLogContent(LOG_ERROR,
            "Failed to write message " + std::to_string(message.id) + " to disk: " + error_msg);
        return false;
    }
}    std::optional<IpcMessage> PersistentMessageQueue::read_from_disk() {
    std::lock_guard<std::mutex> lock(disk_mutex_);

    if (disk_read_pos_ >= disk_index_.size()) {
        return std::nullopt; // 磁盘队列为空
    }

    auto& [offset, size] = disk_index_[disk_read_pos_];

    try {
        if (!data_reader_.is_open()) {
            data_reader_.open(data_file_path_, std::ios::binary);
        }

        data_reader_.seekg(offset);
        std::vector<uint8_t> buffer(size);
        data_reader_.read(reinterpret_cast<char*>(buffer.data()), size);

        auto message = deserialize_message(buffer);
        if (message.has_value()) {
            ++disk_read_pos_;
            current_disk_size_.fetch_sub(size, std::memory_order_relaxed);
        }

        return message;
    }
    catch (const std::exception& e) {
        std::string error_msg = UniConv::GetInstance()->ToUtf8FromLocale(e.what());
        g_LogMessageQueue.WriteLogContent(LOG_ERROR,
            "Failed to read message from disk at offset " + std::to_string(offset) + ": " + error_msg);
        return std::nullopt;
    }
}    void PersistentMessageQueue::rebuild_disk_index() {
    std::lock_guard<std::mutex> lock(disk_mutex_);

    try {
        std::ifstream reader(data_file_path_, std::ios::binary);
        if (!reader.is_open()) {
            return;
        }

        disk_index_.clear();
        loaded_messages_info_.clear();
        uint64_t offset = 0;

        while (reader.good()) {
            // 读取消息头(id, timestamp, priority, data_size)
            uint64_t id, timestamp;
            uint32_t priority, data_size;

            reader.read(reinterpret_cast<char*>(&id), sizeof(id));
            if (reader.gcount() != sizeof(id))
                break;

            reader.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
            reader.read(reinterpret_cast<char*>(&priority), sizeof(priority));
            reader.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));

            // 保存消息信息用于日志输出
            MessageInfo info;
            info.id = id;
            info.timestamp = timestamp;
            info.priority = priority;
            info.data_size = data_size;
            loaded_messages_info_.push_back(info);

            // 跳过数据部分
            reader.seekg(data_size, std::ios::cur);

            // 计算整个消息大小
            uint32_t total_size =
                sizeof(id) + sizeof(timestamp) + sizeof(priority) + sizeof(data_size) + data_size;

            disk_index_.emplace_back(offset, total_size);
            offset += total_size;
        }

        current_disk_size_.store(offset, std::memory_order_release);
        disk_read_pos_ = 0;

        reader.close();
    }
    catch (const std::exception& e) {
        std::string error_msg = UniConv::GetInstance()->ToUtf8FromLocale(e.what());
        g_LogMessageQueue.WriteLogContent(LOG_ERROR,
            "Failed to rebuild disk index: " + error_msg);
    }
}    std::vector<uint8_t> PersistentMessageQueue::serialize_message(const IpcMessage& message) {
    std::vector<uint8_t> buffer;
    buffer.reserve(sizeof(message.id) + sizeof(message.timestamp) + sizeof(message.priority) +
        sizeof(uint32_t) + message.data.size());

    // 写入头部
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&message.id),
        reinterpret_cast<const uint8_t*>(&message.id) + sizeof(message.id));
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&message.timestamp),
        reinterpret_cast<const uint8_t*>(&message.timestamp) + sizeof(message.timestamp));
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&message.priority),
        reinterpret_cast<const uint8_t*>(&message.priority) + sizeof(message.priority));

    // 写入数据长度
    uint32_t data_size = static_cast<uint32_t>(message.data.size());
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&data_size),
        reinterpret_cast<const uint8_t*>(&data_size) + sizeof(data_size));

    // 写入数据
    buffer.insert(buffer.end(), message.data.begin(), message.data.end());

    return buffer;
}

std::optional<IpcMessage> PersistentMessageQueue::deserialize_message(
    const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(uint64_t) * 2 + sizeof(uint32_t) * 2) {
        return std::nullopt;
    }

    IpcMessage message;
    size_t offset = 0;

    // 读取头部
    std::memcpy(&message.id, data.data() + offset, sizeof(message.id));
    offset += sizeof(message.id);

    std::memcpy(&message.timestamp, data.data() + offset, sizeof(message.timestamp));
    offset += sizeof(message.timestamp);

    std::memcpy(&message.priority, data.data() + offset, sizeof(message.priority));
    offset += sizeof(message.priority);

    // 读取数据长度
    uint32_t data_size;
    std::memcpy(&data_size, data.data() + offset, sizeof(data_size));
    offset += sizeof(data_size);

    // 读取数据
    if (offset + data_size != data.size()) {
        return std::nullopt; // 数据长度不匹配
    }

    message.data.assign(data.begin() + offset, data.end());

    return message;
}

