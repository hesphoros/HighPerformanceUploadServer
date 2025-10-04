#ifndef CONNECTION_MONITOR_H
#define CONNECTION_MONITOR_H

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include "utils/EnumConvert.hpp"

// 前向声明，避免包含 <system_error> 导致的依赖问题
namespace std
{
    class error_code;
}

/**
 * @brief 连接状态枚举
 */
enum class ConnectionState
{
    Disconnected,  // 未连接
    Connecting,    // 连接中
    Connected,     // 已连接
    Reconnecting,  // 重连中
    Failed         // 连接失败(永久)
};

/**
 * @brief 错误类型分类
 */
enum class ErrorCategory
{
    NetworkDisconnect,    // 网络断开 (connection_reset, connection_aborted)
    ConnectionRefused,    // 连接被拒绝
    Timeout,              // 超时 (read/write timeout, heartbeat timeout)
    HostUnreachable,      // 主机不可达
    ApplicationError,     // 应用层错误 (非网络错误)
    HeartbeatFailure,     // 心跳失败
    Unknown               // 未知错误
};

/**
 * @brief 错误统计信息
 */
struct ErrorStatistics
{
    std::map<ErrorCategory, uint32_t> error_count_by_category;  ///< 按类型统计错误次数
    std::map<int, uint32_t> error_count_by_code;                ///< 按错误码统计次数

    uint32_t total_network_errors = 0;      ///< 总网络错误次数
    uint32_t total_timeout_errors = 0;      ///< 总超时错误次数
    uint32_t total_heartbeat_failures = 0;  ///< 总心跳失败次数

    uint64_t last_error_time_ms = 0;        ///< 最后一次错误时间
    ErrorCategory last_error_category = ErrorCategory::Unknown;  ///< 最后一次错误类型
    int last_error_code = 0;                ///< 最后一次错误码
};

/**
 * @brief 连接统计信息
 */
struct ConnectionStatistics
{
    uint64_t total_connections = 0;         ///< 总连接次数
    uint64_t successful_connections = 0;    ///< 成功连接次数
    uint64_t failed_connections = 0;        ///< 失败连接次数
    uint64_t total_reconnects = 0;          ///< 总重连次数
    uint64_t total_send_count = 0;          ///< 总发送次数
    uint64_t total_send_success = 0;        ///< 总发送成功次数
    uint64_t total_send_failure = 0;        ///< 总发送失败次数

    uint64_t first_connect_time_ms = 0;     ///< 首次连接时间
    uint64_t last_connect_time_ms = 0;      ///< 最后连接时间
    uint64_t last_disconnect_time_ms = 0;   ///< 最后断开时间
    uint64_t total_connected_duration_ms = 0; ///< 总连接持续时长
};

// 使用宏定义枚举转换支持
DEFINE_ENUM_SUPPORT(ConnectionState)
DEFINE_ENUM_SUPPORT(ErrorCategory)



/**
 * @brief 连接监测器 (增强版)
 *
 * 职责:
 * 1. 监测连接状态变化
 * 2. 错误分类和统计
 * 3. 判断是否需要重连
 * 4. 防止重复触发重连
 * 5. 提供详细的连接和错误统计
 */
    class ConnectionMonitor
{
public:
    using StateChangeCallback = std::function<void(ConnectionState, ConnectionState)>;
    using ReconnectCallback = std::function<void()>;

    ConnectionMonitor();
    ~ConnectionMonitor() = default;

    // ==================== 回调设置 ====================

    /**
     * @brief 设置状态变化回调
     */
    void set_state_change_callback(StateChangeCallback callback);

    /**
     * @brief 设置重连回调
     */
    void set_reconnect_callback(ReconnectCallback callback);

    // ==================== 状态管理 ====================

    /**
     * @brief 更新连接状态
     */
    void set_state(ConnectionState new_state);

    /**
     * @brief 获取当前状态
     */
    ConnectionState get_state() const;

    // ==================== 核心功能 ====================

    /**
     * @brief 记录连接尝试
     */
    void record_connect_attempt();

    /**
     * @brief 记录连接成功
     */
    void record_connect_success();

    /**
     * @brief 记录连接失败
     * @param error 错误码
     */
    void record_connect_failure(const std::error_code& error);

    /**
     * @brief 记录发送成功
     */
    void record_send_success();

    /**
     * @brief 记录发送失败
     * @param error 错误码
     * @param trigger_reconnect 是否立即触发重连 (默认true)
     * @return 是否需要重连
     */
    bool record_send_failure(const std::error_code& error, bool trigger_reconnect = true);

    /**
     * @brief 记录读取失败
     * @param error 错误码
     * @return 是否需要重连
     */
    bool record_read_failure(const std::error_code& error);

    /**
     * @brief 记录心跳失败
     * @param is_timeout 是否为超时失败
     * @return 是否需要重连
     */
    bool record_heartbeat_failure(bool is_timeout = false);

    /**
     * @brief 记录心跳成功 (重置心跳失败计数)
     */
    void record_heartbeat_success();

    // ==================== 错误分类 ====================

    /**
     * @brief 分类错误类型
     * @param error 错误码
     * @return 错误类型
     */
    static ErrorCategory classify_error(const std::error_code& error);

    /**
     * @brief 判断错误是否为网络连接错误
     * @param error 错误码
     * @return true表示需要重连
     */
    static bool is_connection_error(const std::error_code& error);

    /**
     * @brief 判断是否应该触发重连
     * @param category 错误类型
     * @return true表示需要重连
     */
    static bool should_trigger_reconnect(ErrorCategory category);

    // ==================== 统计信息 ====================

    /**
     * @brief 重置统计信息
     */
    void reset_statistics();

    /**
     * @brief 获取连续失败次数
     */
    uint32_t get_consecutive_failures() const;

    /**
     * @brief 获取最后活跃时间
     */
    std::chrono::steady_clock::time_point get_last_active_time() const;

    /**
     * @brief 获取错误统计信息
     */
    ErrorStatistics get_error_statistics() const;

    /**
     * @brief 获取连接统计信息
     */
    ConnectionStatistics get_connection_statistics() const;

    /**
     * @brief 打印统计报告
     */
    void print_statistics_report() const;

    // ==================== 防重复机制 ====================

    /**
     * @brief 检查是否正在重连中
     */
    bool is_reconnecting() const;

    /**
     * @brief 尝试触发重连 (防重复)
     * @return true表示成功触发,false表示已在重连中
     */
    bool try_trigger_reconnect();

    /**
     * @brief 重连完成后调用 (重置重连标志)
     */
    void reconnect_completed();

private:
    // ==================== 内部方法 ====================

    /**
     * @brief 记录错误到统计
     */
    void record_error_to_statistics(const std::error_code& error, ErrorCategory category);

    /**
     * @brief 获取当前时间戳(毫秒)
     */
    static uint64_t get_current_time_ms();

    // ==================== 状态和标志 ====================

    std::atomic<ConnectionState>    state_{ ConnectionState::Disconnected };  ///< 当前连接状态
    std::atomic<bool>               is_reconnecting_flag_{ false };           ///< 是否正在重连
    std::atomic<uint32_t>           consecutive_failures_{ 0 };               ///< 连续失败次数
    std::atomic<uint32_t>           consecutive_heartbeat_failures_{ 0 };     ///< 连续心跳失败次数
    std::atomic<uint64_t>           last_active_time_ms_{ 0 };                ///< 最后活跃时间

    // ==================== 统计数据 ====================

    mutable std::mutex              statistics_mutex_;                        ///< 统计数据互斥锁
    ErrorStatistics                 error_stats_;                             ///< 错误统计
    ConnectionStatistics            connection_stats_;                        ///< 连接统计

    // ==================== 回调函数 ====================

    StateChangeCallback state_change_callback_;
    ReconnectCallback reconnect_callback_;
};

#endif // CONNECTION_MONITOR_H

