#ifndef CONNECTION_MONITOR_H
#define CONNECTION_MONITOR_H

#include <atomic>
#include <chrono>
#include <functional>
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

// 使用宏定义枚举转换支持
DEFINE_ENUM_SUPPORT(ConnectionState)



/**
 * @brief 连接监测器
 *
 * 职责:
 * 1. 监测连接状态变化
 * 2. 检测发送失败原因
 * 3. 判断是否需要重连
 * 4. 触发重连回调
 */
class ConnectionMonitor
{
public:
    using StateChangeCallback = std::function<void(ConnectionState, ConnectionState)>;
    using ReconnectCallback = std::function<void()>;

    ConnectionMonitor();
    ~ConnectionMonitor() = default;

    /**
     * @brief 设置状态变化回调
     */
    void set_state_change_callback(StateChangeCallback callback);

    /**
     * @brief 设置重连回调
     */
    void set_reconnect_callback(ReconnectCallback callback);

    /**
     * @brief 更新连接状态
     */
    void set_state(ConnectionState new_state);

    /**
     * @brief 获取当前状态
     */
    ConnectionState get_state() const;

    /**
     * @brief 记录发送成功
     */
    void record_send_success();

    /**
     * @brief 记录发送失败
     * @param error 错误码
     * @return 是否需要触发重连
     */
    bool record_send_failure(const std::error_code& error);

    /**
     * @brief 判断错误是否为网络连接错误
     * @param error 错误码
     * @return true表示需要重连
     */
    static bool is_connection_error(const std::error_code& error);

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

private:
    std::atomic<ConnectionState>    state_{ ConnectionState::Disconnected };  ///< 当前连接状态
    std::atomic<uint32_t>           consecutive_failures_{ 0 };               ///< 连续失败次数
    std::atomic<uint64_t>           total_send_count_{ 0 };                   ///< 总发送次数
    std::atomic<uint64_t>           total_failure_count_{ 0 };                ///< 总失败次数
    std::atomic<uint64_t>           last_active_time_ms_{ 0 };                ///< 最后活跃时间

    StateChangeCallback state_change_callback_;
    ReconnectCallback reconnect_callback_;
};

#endif // CONNECTION_MONITOR_H

