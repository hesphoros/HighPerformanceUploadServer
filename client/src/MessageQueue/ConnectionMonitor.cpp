#include "asio/asio.hpp"  // 必须在最前面包含，避免 WinSock 冲突
#include "MessageQueue/ConnectionMonitor.h"
#include "log_headers.h"
#include "UniConv.h"
#include <system_error>



ConnectionMonitor::ConnectionMonitor() {
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
    last_active_time_ms_.store(now_ms, std::memory_order_release);
}

void ConnectionMonitor::set_state_change_callback(StateChangeCallback callback) {
    state_change_callback_ = std::move(callback);
}

void ConnectionMonitor::set_reconnect_callback(ReconnectCallback callback) {
    reconnect_callback_ = std::move(callback);
}

void ConnectionMonitor::set_state(ConnectionState new_state) {
    ConnectionState old_state = state_.exchange(new_state, std::memory_order_acq_rel);

    if (old_state != new_state && state_change_callback_) {
        state_change_callback_(old_state, new_state);
    }

    // 更新活跃时间
    if (new_state == ConnectionState::Connected) {
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
        last_active_time_ms_.store(now_ms, std::memory_order_release);
    }
}

ConnectionState ConnectionMonitor::get_state() const {
    return state_.load(std::memory_order_acquire);
}

void ConnectionMonitor::record_send_success() {
    consecutive_failures_.store(0, std::memory_order_release);
    total_send_count_.fetch_add(1, std::memory_order_relaxed);

    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
    last_active_time_ms_.store(now_ms, std::memory_order_release);
}

bool ConnectionMonitor::record_send_failure(const std::error_code& error) {
    consecutive_failures_.fetch_add(1, std::memory_order_relaxed);
    total_failure_count_.fetch_add(1, std::memory_order_relaxed);

    // 判断是否为连接错误
    bool is_conn_error = is_connection_error(error);

    if (is_conn_error) {
        // 将系统本地编码的错误消息转换为 UTF-8
        std::string error_msg = error.message();
        std::string utf8_error_msg = UniConv::GetInstance()->ToUtf8FromLocale(error_msg);

        g_LogMessageQueue.WriteLogContent(LOG_WARN,
            "Connection error detected: " + utf8_error_msg + " (" + std::to_string(error.value()) + ")");

        // 更新状态为重连中
        ConnectionState current_state = state_.load(std::memory_order_acquire);
        if (current_state == ConnectionState::Connected) {
            set_state(ConnectionState::Reconnecting);
        }

        // 触发重连回调
        if (reconnect_callback_) {
            reconnect_callback_();
        }

        return true; // 需要重连
    }

    return false; // 不需要重连
}

bool ConnectionMonitor::is_connection_error(const std::error_code& error) {
    // ASIO 网络错误码
    if (error.category() == asio::error::get_system_category() ||
        error.category() == asio::error::get_misc_category()) {
        switch (error.value()) {
            // 连接被拒绝
        case asio::error::connection_refused:
            // 连接重置
        case asio::error::connection_reset:
            // 连接中止
        case asio::error::connection_aborted:
            // 网络不可达
        case asio::error::network_unreachable:
            // 主机不可达
        case asio::error::host_unreachable:
            // 网络down
        case asio::error::network_down:
            // 对端关闭
        case asio::error::eof:
            // 已断开
        case asio::error::not_connected:
            // 操作中止
        case asio::error::operation_aborted:
            // Broken pipe (POSIX)
        case asio::error::broken_pipe:
            return true;

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
        case 10061: // WSAECONNREFUSED
        case 10064: // WSAEHOSTDOWN
        case 10065: // WSAEHOSTUNREACH
        case 10050: // WSAENETDOWN
        case 10051: // WSAENETUNREACH
        case 10057: // WSAENOTCONN
        case 10058: // WSAESHUTDOWN
            return true;

        default:
            break;
        }
    }
#endif

    return false;
}

void ConnectionMonitor::reset_statistics() {
    consecutive_failures_.store(0, std::memory_order_release);
    total_send_count_.store(0, std::memory_order_release);
    total_failure_count_.store(0, std::memory_order_release);

    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
    last_active_time_ms_.store(now_ms, std::memory_order_release);
}

uint32_t ConnectionMonitor::get_consecutive_failures() const {
    return consecutive_failures_.load(std::memory_order_acquire);
}

std::chrono::steady_clock::time_point ConnectionMonitor::get_last_active_time() const {
    uint64_t time_ms = last_active_time_ms_.load(std::memory_order_acquire);
    return std::chrono::steady_clock::time_point(std::chrono::milliseconds(time_ms));
}

