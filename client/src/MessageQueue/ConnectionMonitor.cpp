#include "asio/asio.hpp"  // 必须在最前面包含，避免 WinSock 冲突
#include "MessageQueue/ConnectionMonitor.h"
#include "log_headers.h"
#include "utils/SystemErrorUtil.h"
#include <system_error>
#include <sstream>
#include <iomanip>
#include <algorithm>



ConnectionMonitor::ConnectionMonitor() {
    last_active_time_ms_.store(get_current_time_ms(), std::memory_order_release);
}

void ConnectionMonitor::set_state_change_callback(StateChangeCallback callback) {
    state_change_callback_ = std::move(callback);
}

void ConnectionMonitor::set_reconnect_callback(ReconnectCallback callback) {
    reconnect_callback_ = std::move(callback);
}

void ConnectionMonitor::set_state(ConnectionState new_state) {
    ConnectionState old_state = state_.exchange(new_state, std::memory_order_acq_rel);

    if (old_state != new_state) {
        // 触发状态变化回调
        if (state_change_callback_) {
            state_change_callback_(old_state, new_state);
        }

        // 更新统计信息
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        uint64_t now_ms = get_current_time_ms();

        if (new_state == ConnectionState::Connected) {
            last_active_time_ms_.store(now_ms, std::memory_order_release);
            connection_stats_.last_connect_time_ms = now_ms;

            if (connection_stats_.first_connect_time_ms == 0) {
                connection_stats_.first_connect_time_ms = now_ms;
            }
        }
        else if (new_state == ConnectionState::Disconnected) {
            connection_stats_.last_disconnect_time_ms = now_ms;

            // 计算连接持续时长
            if (connection_stats_.last_connect_time_ms > 0) {
                uint64_t duration = now_ms - connection_stats_.last_connect_time_ms;
                connection_stats_.total_connected_duration_ms += duration;
            }
        }
        else if (new_state == ConnectionState::Reconnecting) {
            connection_stats_.total_reconnects++;
            is_reconnecting_flag_.store(true, std::memory_order_release);
        }
    }
}

ConnectionState ConnectionMonitor::get_state() const {
    return state_.load(std::memory_order_acquire);
}

void ConnectionMonitor::record_send_success() {
    consecutive_failures_.store(0, std::memory_order_release);

    std::lock_guard<std::mutex> lock(statistics_mutex_);
    connection_stats_.total_send_count++;
    connection_stats_.total_send_success++;

    last_active_time_ms_.store(get_current_time_ms(), std::memory_order_release);
}

bool ConnectionMonitor::record_send_failure(const std::error_code& error, bool trigger_reconnect) {
    consecutive_failures_.fetch_add(1, std::memory_order_relaxed);

    // 分类错误
    ErrorCategory category = classify_error(error);

    {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        connection_stats_.total_send_count++;
        connection_stats_.total_send_failure++;

        record_error_to_statistics(error, category);
    }

    // 判断是否需要重连
    bool need_reconnect = should_trigger_reconnect(category);

    if (need_reconnect) {
        g_LogConnectionMonitor.WriteLogContent(LOG_WARN,
            "[Monitor] 检测到需要重连的错误: " +
            SystemErrorUtil::GetErrorMessage(error) +
            " [类型: " + std::string(magic_enum::enum_name(category)) + "]");

        // 更新状态
        ConnectionState current_state = state_.load(std::memory_order_acquire);
        if (current_state == ConnectionState::Connected) {
            set_state(ConnectionState::Reconnecting);
        }

        // 触发重连回调
        if (trigger_reconnect && reconnect_callback_) {
            if (try_trigger_reconnect()) {
                g_LogConnectionMonitor.WriteLogContent(LOG_INFO,
                    "[Monitor] 触发重连回调");
            }
        }
    }

    return need_reconnect;
}

bool ConnectionMonitor::is_connection_error(const std::error_code& error) {
    ErrorCategory category = classify_error(error);
    return should_trigger_reconnect(category);
}

ErrorCategory ConnectionMonitor::classify_error(const std::error_code& error) {
    // ASIO 网络错误码
    if (error.category() == asio::error::get_system_category() ||
        error.category() == asio::error::get_misc_category()) {

        switch (error.value()) {
            // 连接被拒绝
        case asio::error::connection_refused:
            return ErrorCategory::ConnectionRefused;

            // 连接重置/中止
        case asio::error::connection_reset:
        case asio::error::connection_aborted:
        case asio::error::broken_pipe:
        case asio::error::eof:
            return ErrorCategory::NetworkDisconnect;

            // 超时
        case asio::error::timed_out:
            return ErrorCategory::Timeout;

            // 主机/网络不可达
        case asio::error::network_unreachable:
        case asio::error::host_unreachable:
        case asio::error::network_down:
            return ErrorCategory::HostUnreachable;

            // 未连接
        case asio::error::not_connected:
            return ErrorCategory::NetworkDisconnect;

        default:
            break;
        }
    }

#ifdef _WIN32
    // Windows 特定错误码
    if (error.category() == std::system_category()) {
        switch (error.value()) {
        case 10053: // WSAECONNABORTED
        case 10054: // WSAECONNRESET
        case 1236:  // ERROR_CONNECTION_ABORTED
            return ErrorCategory::NetworkDisconnect;

        case 10061: // WSAECONNREFUSED
            return ErrorCategory::ConnectionRefused;

        case 10060: // WSAETIMEDOUT
            return ErrorCategory::Timeout;

        case 10064: // WSAEHOSTDOWN
        case 10065: // WSAEHOSTUNREACH
        case 10050: // WSAENETDOWN
        case 10051: // WSAENETUNREACH
            return ErrorCategory::HostUnreachable;

        case 10057: // WSAENOTCONN
        case 10058: // WSAESHUTDOWN
            return ErrorCategory::NetworkDisconnect;

        default:
            break;
        }
    }
#endif

    // 其他错误归类为应用层错误
    return ErrorCategory::ApplicationError;
}

bool ConnectionMonitor::should_trigger_reconnect(ErrorCategory category) {
    switch (category) {
    case ErrorCategory::NetworkDisconnect:
    case ErrorCategory::ConnectionRefused:
    case ErrorCategory::HostUnreachable:
    case ErrorCategory::Timeout:
    case ErrorCategory::HeartbeatFailure:
        return true;

    case ErrorCategory::ApplicationError:
    case ErrorCategory::Unknown:
    default:
        return false;
    }
}

void ConnectionMonitor::reset_statistics() {
    consecutive_failures_.store(0, std::memory_order_release);
    consecutive_heartbeat_failures_.store(0, std::memory_order_release);

    std::lock_guard<std::mutex> lock(statistics_mutex_);

    // 保留累计统计,只重置当前会话的统计
    connection_stats_.total_send_count = 0;
    connection_stats_.total_send_success = 0;
    connection_stats_.total_send_failure = 0;

    last_active_time_ms_.store(get_current_time_ms(), std::memory_order_release);
}

uint32_t ConnectionMonitor::get_consecutive_failures() const {
    return consecutive_failures_.load(std::memory_order_acquire);
}

std::chrono::steady_clock::time_point ConnectionMonitor::get_last_active_time() const {
    uint64_t time_ms = last_active_time_ms_.load(std::memory_order_acquire);
    return std::chrono::steady_clock::time_point(std::chrono::milliseconds(time_ms));
}

// ==================== 新增连接记录方法 ====================

void ConnectionMonitor::record_connect_attempt() {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    connection_stats_.total_connections++;
}

void ConnectionMonitor::record_connect_success() {
    consecutive_failures_.store(0, std::memory_order_release);
    consecutive_heartbeat_failures_.store(0, std::memory_order_release);

    std::lock_guard<std::mutex> lock(statistics_mutex_);
    connection_stats_.successful_connections++;

    last_active_time_ms_.store(get_current_time_ms(), std::memory_order_release);
}

void ConnectionMonitor::record_connect_failure(const std::error_code& error) {
    consecutive_failures_.fetch_add(1, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(statistics_mutex_);
    connection_stats_.failed_connections++;

    // 分类并记录错误
    ErrorCategory category = classify_error(error);
    record_error_to_statistics(error, category);
}

// ==================== 读取和心跳记录 ====================

bool ConnectionMonitor::record_read_failure(const std::error_code& error) {
    // 读取失败通常意味着连接断开,需要重连
    return record_send_failure(error, true);
}

bool ConnectionMonitor::record_heartbeat_failure(bool is_timeout) {
    uint32_t failure_count = consecutive_heartbeat_failures_.fetch_add(1, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(statistics_mutex_);
    error_stats_.total_heartbeat_failures++;

    if (is_timeout) {
        error_stats_.total_timeout_errors++;
        error_stats_.error_count_by_category[ErrorCategory::Timeout]++;
        error_stats_.last_error_category = ErrorCategory::Timeout;
    }
    else {
        error_stats_.error_count_by_category[ErrorCategory::HeartbeatFailure]++;
        error_stats_.last_error_category = ErrorCategory::HeartbeatFailure;
    }

    error_stats_.last_error_time_ms = get_current_time_ms();

    g_LogConnectionMonitor.WriteLogContent(LOG_WARN,
        "[Monitor] 心跳失败 #" + std::to_string(failure_count + 1) +
        (is_timeout ? " (超时)" : " (发送失败)"));

    // 心跳失败也应该触发重连
    return true;
}

void ConnectionMonitor::record_heartbeat_success() {
    consecutive_heartbeat_failures_.store(0, std::memory_order_release);
    last_active_time_ms_.store(get_current_time_ms(), std::memory_order_release);
}

// ==================== 统计信息获取 ====================

ErrorStatistics ConnectionMonitor::get_error_statistics() const {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    return error_stats_;
}

ConnectionStatistics ConnectionMonitor::get_connection_statistics() const {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    return connection_stats_;
}

void ConnectionMonitor::print_statistics_report() const {
    std::lock_guard<std::mutex> lock(statistics_mutex_);

    std::stringstream ss;
    ss << "\n==================== 连接监测统计报告 ====================\n";

    // 连接统计
    ss << "[连接统计]\n";
    ss << "  总连接次数: " << connection_stats_.total_connections << "\n";
    ss << "  成功连接: " << connection_stats_.successful_connections << "\n";
    ss << "  失败连接: " << connection_stats_.failed_connections << "\n";
    ss << "  总重连次数: " << connection_stats_.total_reconnects << "\n";

    if (connection_stats_.total_connected_duration_ms > 0) {
        double hours = connection_stats_.total_connected_duration_ms / 3600000.0;
        ss << "  累计连接时长: " << std::fixed << std::setprecision(2) << hours << " 小时\n";
    }

    // 发送统计
    ss << "\n[发送统计]\n";
    ss << "  总发送次数: " << connection_stats_.total_send_count << "\n";
    ss << "  发送成功: " << connection_stats_.total_send_success << "\n";
    ss << "  发送失败: " << connection_stats_.total_send_failure << "\n";

    if (connection_stats_.total_send_count > 0) {
        double success_rate = (double)connection_stats_.total_send_success / connection_stats_.total_send_count * 100.0;
        ss << "  成功率: " << std::fixed << std::setprecision(2) << success_rate << "%\n";
    }

    // 错误统计
    ss << "\n[错误统计]\n";
    ss << "  总网络错误: " << error_stats_.total_network_errors << "\n";
    ss << "  总超时错误: " << error_stats_.total_timeout_errors << "\n";
    ss << "  总心跳失败: " << error_stats_.total_heartbeat_failures << "\n";

    if (!error_stats_.error_count_by_category.empty()) {
        ss << "\n[按类型分类]\n";
        for (const auto& [category, count] : error_stats_.error_count_by_category) {
            ss << "  " << std::string(magic_enum::enum_name(category)) << ": " << count << "\n";
        }
    }

    if (!error_stats_.error_count_by_code.empty()) {
        ss << "\n[按错误码分类 (Top 5)]\n";
        std::vector<std::pair<int, uint32_t>> sorted_errors(
            error_stats_.error_count_by_code.begin(),
            error_stats_.error_count_by_code.end());

        std::sort(sorted_errors.begin(), sorted_errors.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        int count = 0;
        for (const auto& [code, cnt] : sorted_errors) {
            if (count++ >= 5) break;
            ss << "  错误码 " << code << ": " << cnt << " 次\n";
        }
    }

    // 当前状态
    ss << "\n[当前状态]\n";
    ss << "  连接状态: " << std::string(magic_enum::enum_name(state_.load())) << "\n";
    ss << "  连续失败次数: " << consecutive_failures_.load() << "\n";
    ss << "  连续心跳失败: " << consecutive_heartbeat_failures_.load() << "\n";
    ss << "  是否在重连中: " << (is_reconnecting_flag_.load() ? "是" : "否") << "\n";

    if (error_stats_.last_error_time_ms > 0) {
        ss << "  最后错误类型: " << std::string(magic_enum::enum_name(error_stats_.last_error_category)) << "\n";
        ss << "  最后错误码: " << error_stats_.last_error_code << "\n";
    }

    ss << "========================================================\n";

    g_LogConnectionMonitor.WriteLogContent(LOG_INFO, ss.str());
}

// ==================== 防重复机制 ====================

bool ConnectionMonitor::is_reconnecting() const {
    return is_reconnecting_flag_.load(std::memory_order_acquire);
}

bool ConnectionMonitor::try_trigger_reconnect() {
    bool expected = false;
    if (is_reconnecting_flag_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        // 成功设置重连标志,触发回调
        if (reconnect_callback_) {
            reconnect_callback_();
        }
        return true;
    }

    // 已经在重连中，跳过
    return false;
}

void ConnectionMonitor::reconnect_completed() {
    is_reconnecting_flag_.store(false, std::memory_order_release);
}

// ==================== 内部方法 ====================

void ConnectionMonitor::record_error_to_statistics(const std::error_code& error, ErrorCategory category) {
    // 注意: 调用此函数前必须已持有 statistics_mutex_ 锁

    // 按类型统计
    error_stats_.error_count_by_category[category]++;

    // 按错误码统计
    error_stats_.error_count_by_code[error.value()]++;

    // 更新特定类型计数
    switch (category) {
    case ErrorCategory::NetworkDisconnect:
    case ErrorCategory::ConnectionRefused:
    case ErrorCategory::HostUnreachable:
        error_stats_.total_network_errors++;
        break;

    case ErrorCategory::Timeout:
        error_stats_.total_timeout_errors++;
        break;

    default:
        break;
    }

    // 更新最后错误信息
    error_stats_.last_error_time_ms = get_current_time_ms();
    error_stats_.last_error_category = category;
    error_stats_.last_error_code = error.value();
}

uint64_t ConnectionMonitor::get_current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

