#include "MessageQueue/PersistentMessageQueue.h"
#include "log_headers.h"
#include "utils/SystemErrorUtil.h"
#include "UniConv.h"
#include <algorithm>
#include <chrono>
#include <cstring>
// 尾部使用CRC32校验
#include "crc32.h"


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
            "Initializing PersistentMessageQueue: capacity=" + std::to_string(memory_capacity_) +
            ", max_disk=" + std::to_string(max_disk_size_) + " bytes, path=" + data_file_path_.string());

        // 加载已有数据（优先使用索引文件）
        if (std::filesystem::exists(data_file_path_)) {
            rebuild_disk_index();
            g_LogMessageQueue.WriteLogContent(LOG_INFO,
                "Loaded " + std::to_string(disk_index_.size()) + " messages from disk");
        }


    }
    catch (const std::exception& e) {
        std::string error_msg = UniConv::GetInstance()->ToUtf8FromLocale(e.what());
        g_LogMessageQueue.WriteLogContent(LOG_ERROR,
            "Failed to initialize PersistentMessageQueue: " + error_msg);
    }
}


PersistentMessageQueue::~PersistentMessageQueue() {
    try {
        // 获取统计信息
        auto stats = get_statistics();

        if (stats.memory_size > 0) {
            g_LogMessageQueue.WriteLogContent(LOG_INFO,
                "Flushing " + std::to_string(stats.memory_size) + " messages to disk...");

            size_t flushed = flush_to_disk();
            if (flushed > 0) {
                g_LogMessageQueue.WriteLogContent(LOG_INFO,
                    "Successfully flushed " + std::to_string(flushed) + " messages");
            }

            // 重新获取最终统计
            stats = get_statistics();
        }

        // 保存索引文件（加速下次启动）
        if (stats.disk_size > 0) {
            if (save_disk_index()) {
                g_LogMessageQueue.WriteLogContent(LOG_INFO,
                    "Saved disk index (" + std::to_string(stats.disk_size) + " entries) to " + index_file_path_.string());
            }
        }

        // 记录最终统计
        g_LogMessageQueue.WriteLogContent(LOG_INFO,
            "Final stats - Memory: " + std::to_string(stats.memory_size) +
            ", Disk: " + std::to_string(stats.disk_size) + " (" + std::to_string(stats.disk_bytes) + " bytes)" +
            ", Enqueued: " + std::to_string(stats.total_enqueued) +
            ", Dequeued: " + std::to_string(stats.total_dequeued));

        // 关闭文件
        if (data_writer_.is_open()) data_writer_.close();
        if (data_reader_.is_open()) data_reader_.close();

    }
    catch (const std::exception& e) {
        g_LogMessageQueue.WriteLogContent(LOG_ERROR,
            "Destructor exception: " + UniConv::GetInstance()->ToUtf8FromLocale(e.what()));
    }
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

std::optional<IpcMessage> PersistentMessageQueue::peek() const {
    size_t read_idx = read_pos_.load(std::memory_order_acquire);
    size_t write_idx = write_pos_.load(std::memory_order_acquire);

    // 内存队列为空
    if (read_idx == write_idx) {
        // 尝试从磁盘读取（只读，不改变 disk_read_pos_）
        std::lock_guard<std::mutex> lock(disk_mutex_);
        if (disk_read_pos_ >= disk_index_.size()) {
            return std::nullopt;
        }

        auto& [offset, size] = disk_index_[disk_read_pos_];
        try {
            // 创建临时读取器
            std::ifstream temp_reader(data_file_path_, std::ios::binary);
            if (!temp_reader.is_open()) {
                return std::nullopt;
            }

            temp_reader.seekg(offset);
            std::vector<uint8_t> buffer(size);
            temp_reader.read(reinterpret_cast<char*>(buffer.data()), size);
            temp_reader.close();

            return deserialize_message(buffer);
        }
        catch (const std::exception& e) {
            std::string error_msg = UniConv::GetInstance()->ToUtf8FromLocale(e.what());
            g_LogMessageQueue.WriteLogContent(LOG_ERROR,
                "Failed to peek message from disk at offset " + std::to_string(offset) + ": " + error_msg);
            return std::nullopt;
        }
    }

    // 等待消息写入完成
    while (!memory_buffer_[read_idx].ready.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // 复制消息（不移动）
    return memory_buffer_[read_idx].message;
}

bool PersistentMessageQueue::pop_front() {
    size_t read_idx = read_pos_.load(std::memory_order_relaxed);
    size_t write_idx = write_pos_.load(std::memory_order_acquire);

    // 内存队列为空，尝试从磁盘删除
    if (read_idx == write_idx) {
        std::lock_guard<std::mutex> lock(disk_mutex_);
        if (disk_read_pos_ >= disk_index_.size()) {
            return false; // 队列为空
        }

        auto& [offset, size] = disk_index_[disk_read_pos_];
        ++disk_read_pos_;
        current_disk_size_.fetch_sub(size, std::memory_order_relaxed);
        total_dequeued_.fetch_add(1, std::memory_order_relaxed);

        g_LogMessageQueue.WriteLogContent(LOG_DEBUG,
            "Popped message, remaining messages: " + std::to_string(disk_index_.size() - disk_read_pos_));
        return true;
    }

    // 等待消息写入完成
    while (!memory_buffer_[read_idx].ready.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // 标记为未就绪
    memory_buffer_[read_idx].ready.store(false, std::memory_order_release);

    // 更新读指针
    read_pos_.store(next_index(read_idx), std::memory_order_release);
    total_dequeued_.fetch_add(1, std::memory_order_relaxed);

    size_t remaining = size();
    g_LogMessageQueue.WriteLogContent(LOG_DEBUG,
        "Popped message, remaining messages: " + std::to_string(remaining));
    return true;
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
            }

            // 写入内存队列(不经过enqueue避免重复ID分配)
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
}

size_t PersistentMessageQueue::flush_to_disk() {
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
}

std::optional<IpcMessage> PersistentMessageQueue::read_from_disk() {
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
}

void PersistentMessageQueue::rebuild_disk_index() {
    // 🚀 策略：优先从索引文件快速加载，失败才扫描数据文件
    if (load_disk_index()) {
        g_LogMessageQueue.WriteLogContent(LOG_INFO,
            "Loaded disk index from index file (fast path)");
        return;
    }

    g_LogMessageQueue.WriteLogContent(LOG_WARN,
        "Index file not found or invalid, rebuilding from data file (slow path)");
    rebuild_disk_index_from_data();
}

void PersistentMessageQueue::rebuild_disk_index_from_data() {
    std::lock_guard<std::mutex> lock(disk_mutex_);

    try {
        std::ifstream reader(data_file_path_, std::ios::binary);
        if (!reader.is_open()) {
            return;
        }

        disk_index_.clear();
        uint64_t offset = 0;

        // 消息头固定大小: id(8) + timestamp(8) + priority(4) + data_size(4) = 24 字节
        constexpr uint32_t HEADER_SIZE = sizeof(uint64_t) * 2 + sizeof(uint32_t) * 2;

        while (reader.good()) {
            // 只需要读取 data_size 字段（跳过前 20 字节）
            reader.seekg(offset + 20, std::ios::beg);  // 跳过 id(8) + timestamp(8) + priority(4)

            uint32_t data_size = 0;
            reader.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));

            if (reader.gcount() != sizeof(data_size)) {
                break;  // 文件结束或读取失败
            }

            // 计算整个消息大小并添加索引
            uint32_t total_size = HEADER_SIZE + data_size;
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
}

std::vector<uint8_t> PersistentMessageQueue::serialize_message(const IpcMessage& message) {
    // 消息头固定大小: id(8) + timestamp(8) + priority(4) + data_size(4) = 24 字节
    constexpr size_t HEADER_SIZE = sizeof(uint64_t) * 2 + sizeof(uint32_t) * 2;

    const uint32_t data_size = static_cast<uint32_t>(message.data.size());
    const size_t total_size = HEADER_SIZE + data_size;

    std::vector<uint8_t> buffer(total_size);
    size_t offset = 0;

    // 使用 memcpy 写入头部（性能优于 insert）
    std::memcpy(buffer.data() + offset, &message.id, sizeof(message.id));
    offset += sizeof(message.id);

    std::memcpy(buffer.data() + offset, &message.timestamp, sizeof(message.timestamp));
    offset += sizeof(message.timestamp);

    std::memcpy(buffer.data() + offset, &message.priority, sizeof(message.priority));
    offset += sizeof(message.priority);

    std::memcpy(buffer.data() + offset, &data_size, sizeof(data_size));
    offset += sizeof(data_size);

    // 写入数据
    if (data_size > 0) {
        std::memcpy(buffer.data() + offset, message.data.data(), data_size);
    }

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

// ========== 索引文件持久化实现 ==========

bool PersistentMessageQueue::load_disk_index() {
    std::lock_guard<std::mutex> lock(disk_mutex_);

    try {
        if (!std::filesystem::exists(index_file_path_)) {
            return false;
        }

        std::ifstream index_reader(index_file_path_, std::ios::binary);
        if (!index_reader.is_open()) {
            return false;
        }

        // 读取文件头
        constexpr uint32_t MAGIC_NUMBER = 0x4D515549;  // "MQUI"
        constexpr uint32_t VERSION = 1;

        uint32_t magic, version;
        uint64_t message_count, total_data_size;

        index_reader.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        index_reader.read(reinterpret_cast<char*>(&version), sizeof(version));
        index_reader.read(reinterpret_cast<char*>(&message_count), sizeof(message_count));
        index_reader.read(reinterpret_cast<char*>(&total_data_size), sizeof(total_data_size));

        // 验证魔数和版本
        if (magic != MAGIC_NUMBER || version != VERSION) {
            g_LogMessageQueue.WriteLogContent(LOG_WARN,
                "Index file has invalid magic/version, rebuilding...");
            return false;
        }

        // 验证数据文件大小是否匹配
        auto actual_data_size = std::filesystem::file_size(data_file_path_);
        if (actual_data_size != total_data_size) {
            g_LogMessageQueue.WriteLogContent(LOG_WARN,
                "Data file size mismatch (expected " + std::to_string(total_data_size) +
                ", actual " + std::to_string(actual_data_size) + "), rebuilding index...");
            return false;
        }

        // 读取索引条目
        disk_index_.clear();
        disk_index_.reserve(message_count);

        for (uint64_t i = 0; i < message_count; ++i) {
            uint64_t offset;
            uint32_t size;

            index_reader.read(reinterpret_cast<char*>(&offset), sizeof(offset));
            index_reader.read(reinterpret_cast<char*>(&size), sizeof(size));

            disk_index_.emplace_back(offset, size);
        }

        // 🔒 读取并验证 CRC32 校验和
        uint32_t stored_crc = 0;
        index_reader.read(reinterpret_cast<char*>(&stored_crc), sizeof(stored_crc));

        if (index_reader.gcount() != sizeof(stored_crc)) {
            g_LogMessageQueue.WriteLogContent(LOG_WARN,
                "Index file missing CRC32 checksum, rebuilding...");
            disk_index_.clear();
            return false;
        }

        // 计算实际 CRC32（不包括末尾的4字节CRC）
        size_t content_size = index_reader.tellg() - static_cast<std::streamoff>(sizeof(stored_crc));
        index_reader.seekg(0, std::ios::beg);

        std::vector<uint8_t> file_content(content_size);
        index_reader.read(reinterpret_cast<char*>(file_content.data()), content_size);

        CRC32 crc32;
        crc32.add(file_content.data(), file_content.size());
        unsigned char crc_bytes[4];
        crc32.getHash(crc_bytes);
        uint32_t calculated_crc = *reinterpret_cast<uint32_t*>(crc_bytes);

        if (calculated_crc != stored_crc) {
            g_LogMessageQueue.WriteLogContent(LOG_WARN,
                "Index file CRC32 mismatch (expected 0x" + std::to_string(stored_crc) +
                ", got 0x" + std::to_string(calculated_crc) + "), rebuilding...");
            disk_index_.clear();
            return false;
        }

        g_LogMessageQueue.WriteLogContent(LOG_DEBUG,
            "Index file CRC32 verified: 0x" + std::to_string(stored_crc));

        current_disk_size_.store(total_data_size, std::memory_order_release);
        disk_read_pos_ = 0;

        index_reader.close();

        g_LogMessageQueue.WriteLogContent(LOG_INFO,
            "Loaded " + std::to_string(message_count) + " index entries from " + index_file_path_.string());
        return true;

    }
    catch (const std::exception& e) {
        g_LogMessageQueue.WriteLogContent(LOG_ERROR,
            "Failed to load index file: " + UniConv::GetInstance()->ToUtf8FromLocale(e.what()));
        disk_index_.clear();
        return false;
    }
}

bool PersistentMessageQueue::save_disk_index() {
    std::lock_guard<std::mutex> lock(disk_mutex_);

    try {
        // 没有磁盘消息，删除索引文件
        if (disk_index_.empty()) {
            std::filesystem::remove(index_file_path_);
            return true;
        }

        std::ofstream index_writer(index_file_path_, std::ios::binary | std::ios::trunc);
        if (!index_writer.is_open()) {
            g_LogMessageQueue.WriteLogContent(LOG_ERROR,
                "Failed to open index file for writing: " + index_file_path_.string());
            return false;
        }

        // 写入文件头
        constexpr uint32_t MAGIC_NUMBER = 0x4D515549;  // "MQUI"
        constexpr uint32_t VERSION = 1;

        uint64_t message_count = disk_index_.size();
        uint64_t total_data_size = current_disk_size_.load(std::memory_order_relaxed);

        index_writer.write(reinterpret_cast<const char*>(&MAGIC_NUMBER), sizeof(MAGIC_NUMBER));
        index_writer.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));
        index_writer.write(reinterpret_cast<const char*>(&message_count), sizeof(message_count));
        index_writer.write(reinterpret_cast<const char*>(&total_data_size), sizeof(total_data_size));

        // 写入索引条目
        for (const auto& [offset, size] : disk_index_) {
            index_writer.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
            index_writer.write(reinterpret_cast<const char*>(&size), sizeof(size));
        }

        // 🔒 计算 CRC32 校验和（对整个文件内容）
        index_writer.flush();
        size_t file_size = index_writer.tellp();
        index_writer.close();

        // 重新读取文件内容计算 CRC32
        std::ifstream temp_reader(index_file_path_, std::ios::binary);
        std::vector<uint8_t> file_content(file_size);
        temp_reader.read(reinterpret_cast<char*>(file_content.data()), file_size);
        temp_reader.close();

        CRC32 crc32;
        crc32.add(file_content.data(), file_content.size());
        unsigned char crc_bytes[4];
        crc32.getHash(crc_bytes);
        uint32_t crc_value = *reinterpret_cast<uint32_t*>(crc_bytes);

        // 将 CRC32 附加到文件末尾
        std::ofstream crc_writer(index_file_path_, std::ios::binary | std::ios::app);
        crc_writer.write(reinterpret_cast<const char*>(&crc_value), sizeof(crc_value));
        crc_writer.flush();
        crc_writer.close();

        g_LogMessageQueue.WriteLogContent(LOG_DEBUG,
            "Saved index with CRC32: 0x" + std::to_string(crc_value));

        return true;

    }
    catch (const std::exception& e) {
        g_LogMessageQueue.WriteLogContent(LOG_ERROR,
            "Failed to save index file: " + UniConv::GetInstance()->ToUtf8FromLocale(e.what()));
        return false;
    }
}



