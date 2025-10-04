#include "AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.h"
#include "Config/ClientConfigManager.h"
#include "log_headers.h"
#include "utils/SystemErrorUtil.h"
#include "upload_file_info_generated.h"
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <mstcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#endif


Lusp_AsioLoopbackIpcClient::Lusp_AsioLoopbackIpcClient(asio::io_context& io_context, const ClientConfigManager& configMgr)
    : io_context_(io_context)
    , config_mgr_(configMgr)
    , socket_(std::make_shared<asio::ip::tcp::socket>(io_context))
    , current_reconnect_attempts_(0)
    , is_connecting_(false)
    , is_permanently_stopped_(false)
    , reconnect_timer_(std::make_shared<asio::steady_timer>(io_context))
    , heartbeat_timer_(std::make_shared<asio::steady_timer>(io_context))
    , client_computer_name_(get_computer_name()) {

    const auto& networkConfig = config_mgr_.getNetworkConfig();
    // 缓冲区大小
    buffer_ = std::make_shared<std::vector<char>>(networkConfig.bufferSize);

    // 初始化消息队列（持久化目录：./queue，内存容量1024，磁盘最大100MB）
    message_queue_ = std::make_unique<PersistentMessageQueue>(
        std::filesystem::path("./queue"),
        1024,
        100 * 1024 * 1024
    );

    // 初始化连接监测器
    connection_monitor_ = std::make_unique<ConnectionMonitor>();

    // 设置状态变化回调
    connection_monitor_->set_state_change_callback([this](ConnectionState old_state, ConnectionState new_state) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
            "[IPC] 连接状态变化: " + std::string(ConnectionStateToString(old_state)) +
            " -> " + std::string(ConnectionStateToString(new_state)));
        });

    // 设置重连回调
    connection_monitor_->set_reconnect_callback([this]() {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_WARN, "[IPC] 连接监测器触发重连");
        try_reconnect();
        });

    const auto& uploadConfig = config_mgr_.getUploadConfig();
    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
        "[IPC] 初始化客户端: " + uploadConfig.serverHost + ":" + std::to_string(uploadConfig.serverPort));
}


void Lusp_AsioLoopbackIpcClient::connect() {
    if (is_connecting_) {
        return;
    }

    is_connecting_ = true;
    is_permanently_stopped_ = false;  // 重置永久停止标志，允许重新连接
    connection_monitor_->set_state(ConnectionState::Connecting);

    try {
        const auto& uploadConfig = config_mgr_.getUploadConfig();

        // 解析服务器地址 解析端点
        asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(uploadConfig.serverHost, std::to_string(uploadConfig.serverPort));

        asio::async_connect(*socket_, endpoints,
            [this](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
                handle_connect_result(ec, endpoint);
            });

        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
            "[IPC] 尝试连接到 " + uploadConfig.serverHost + ":" + std::to_string(uploadConfig.serverPort));
    }
    catch (const std::exception& e) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
            "[IPC] 连接异常: " + std::string(e.what()));
        is_connecting_ = false;
        connection_monitor_->set_state(ConnectionState::Disconnected);
        try_reconnect();
    }
}

void Lusp_AsioLoopbackIpcClient::send(const std::string& message, uint32_t priority) {
    // 构造消息并入队
    std::vector<uint8_t> data(message.begin(), message.end());
    IpcMessage ipc_message(0, data, priority);

    if (!message_queue_->enqueue(std::move(ipc_message))) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
            "[IPC] 消息队列已满，消息长度: " + std::to_string(message.size()));
        return;
    }

    // 触发发送
    do_send_from_queue();
}

void Lusp_AsioLoopbackIpcClient::do_send_from_queue() {
    // 使用原子操作避免多次并发发送
    bool expected = false;
    if (!is_sending_.compare_exchange_strong(expected, true)) {
        return; // 已经在发送中
    }

    if (!is_connected()) {
        is_sending_.store(false);
        return;
    }

    auto ipc_message_opt = message_queue_->peek();
    if (!ipc_message_opt.has_value()) {
        is_sending_.store(false);
        return;
    }

    const auto& ipc_message = ipc_message_opt.value();

    try {
        // 计算长度并转为4字节（小端序）
        uint32_t len = static_cast<uint32_t>(ipc_message.data.size());
        std::vector<char> buffer(4 + ipc_message.data.size());
        buffer[0] = static_cast<char>(len & 0xFF);
        buffer[1] = static_cast<char>((len >> 8) & 0xFF);
        buffer[2] = static_cast<char>((len >> 16) & 0xFF);
        buffer[3] = static_cast<char>((len >> 24) & 0xFF);

        // 拷贝消息体
        std::memcpy(buffer.data() + 4, ipc_message.data.data(), ipc_message.data.size());

        auto data = std::make_shared<std::vector<char>>(std::move(buffer));
        asio::async_write(*socket_, asio::buffer(*data),
            [this, data, len, msg_id = ipc_message.id](std::error_code ec, std::size_t bytes_sent) {
                handle_send_result(ec, bytes_sent, msg_id);

                if (!ec) {
                    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_DEBUG,
                        "[IPC] 消息 " + std::to_string(msg_id) + " 发送成功，长度: " + std::to_string(len));
                }
                else {
                    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
                        "[IPC] 消息 " + std::to_string(msg_id) + " 发送失败: " + SystemErrorUtil::GetErrorMessage(ec) +
                        "，消息保留在队列等待重试");
                }
            });
    }
    catch (const std::exception& e) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
            "[IPC] 发送异常: " + std::string(e.what()));
        is_sending_.store(false);
        connection_monitor_->record_send_failure(std::make_error_code(std::errc::io_error));
    }
}

void Lusp_AsioLoopbackIpcClient::handle_send_result(const std::error_code& ec, std::size_t bytes_transferred, uint64_t msg_id) {
    is_sending_.store(false);

    if (!ec) {

        if (message_queue_->pop_front()) {
            g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_DEBUG,
                "[IPC] 消息 " + std::to_string(msg_id) + " 已从队列移除");
        }

        connection_monitor_->record_send_success();


        do_send_from_queue();
    }
    else {
        // 发送失败，消息保留在队列，交给连接监测器判断是否需要重连
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_WARN,
            "[IPC] 消息 " + std::to_string(msg_id) + " 发送失败，保留在队列等待重试");

        bool need_reconnect = connection_monitor_->record_send_failure(ec);

        if (!need_reconnect) {
            // 不需要重连，短暂延迟后继续发送（可能是临时错误）
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            do_send_from_queue();
        }
        // 如果需要重连，ConnectionMonitor 会触发 try_reconnect()
    }
}

void Lusp_AsioLoopbackIpcClient::on_receive(MessageCallback cb) {
    on_message_ = std::move(cb);
}

void Lusp_AsioLoopbackIpcClient::disconnect() {
    is_permanently_stopped_ = true;  // 设置永久停止标志，防止自动重连
    connection_monitor_->set_state(ConnectionState::Disconnected);

    // 停止心跳
    stop_heartbeat_timer();

    if (socket_ && socket_->is_open()) {
        try {
            socket_->close();
            g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO, "[IPC] 连接已断开");
        }
        catch (const std::exception& e) {
            g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
                "[IPC] 断开连接异常: " + std::string(e.what()));
        }
    }
}

bool Lusp_AsioLoopbackIpcClient::is_connected() const {
    return socket_ && socket_->is_open();
}

PersistentMessageQueue::Statistics Lusp_AsioLoopbackIpcClient::get_queue_statistics() const {
    return message_queue_->get_statistics();
}

void Lusp_AsioLoopbackIpcClient::do_read() {
    if (!is_connected()) {
        return;
    }

    socket_->async_read_some(asio::buffer(*buffer_),
        [this](std::error_code ec, std::size_t bytes_transferred) {
            handle_read_result(ec, bytes_transferred);
        });
}

void Lusp_AsioLoopbackIpcClient::try_reconnect() {
    const auto& networkConfig = config_mgr_.getNetworkConfig();

    // 检查是否已永久停止
    if (is_permanently_stopped_) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO, "[IPC] 重连已永久停止，请手动调用 connect() 重新连接");
        return;
    }

    if (!networkConfig.enableAutoReconnect) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO, "[IPC] 自动重连已禁用");
        return;
    }

    if (current_reconnect_attempts_ >= static_cast<int>(networkConfig.maxReconnectAttempts)) {
        is_permanently_stopped_ = true;  // 设置永久停止标志
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
            "[IPC] 达到最大重连次数 " + std::to_string(networkConfig.maxReconnectAttempts) + ",永久停止重连");
        return;
    }

    current_reconnect_attempts_++;
    is_connecting_ = false;

    // 创建新的socket
    socket_ = std::make_shared<asio::ip::tcp::socket>(io_context_);

    uint32_t delay = networkConfig.reconnectIntervalMs;

    // 指数退避策略
    if (networkConfig.enableReconnectBackoff && current_reconnect_attempts_ > 1) {
        // 指数退避：delay = base_delay × 2^(attempts-1)
        // 例如：1000ms, 2000ms, 4000ms, 8000ms, 16000ms
        uint32_t exponentialDelay = delay * (1U << (current_reconnect_attempts_ - 1));
        delay = std::min(exponentialDelay, networkConfig.reconnectBackoffMs);

        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_DEBUG,
            "[IPC] 指数退避计算: " + std::to_string(delay) +
            "ms (基础: " + std::to_string(networkConfig.reconnectIntervalMs) +
            "ms x 2^" + std::to_string(current_reconnect_attempts_ - 1) +
            ", 上限: " + std::to_string(networkConfig.reconnectBackoffMs) + "ms)");
    }

    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
        "[IPC] 第 " + std::to_string(current_reconnect_attempts_) +
        "/" + std::to_string(networkConfig.maxReconnectAttempts) +
        " 次重连," + std::to_string(delay) + "ms 后尝试");

    // 使用异步定时器进行重连
    reconnect_timer_->expires_after(std::chrono::milliseconds(delay));
    reconnect_timer_->async_wait([this](std::error_code ec) {
        if (!ec) {
            const auto& networkConfig = config_mgr_.getNetworkConfig();
            if (networkConfig.enableAutoReconnect) {
                connect();
            }
        }
        });
}

void Lusp_AsioLoopbackIpcClient::handle_connect_result(const std::error_code& ec, const asio::ip::tcp::endpoint& endpoint) {
    is_connecting_ = false;

    if (!ec) {
        current_reconnect_attempts_ = 0; // 重置重连计数
        connection_monitor_->set_state(ConnectionState::Connected);
        connection_monitor_->reset_statistics();

        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
            "[IPC] 连接成功: " + endpoint.address().to_string() + ":" + std::to_string(endpoint.port()));

        //  设置 TCP Keep-Alive
        const auto& networkConfig = config_mgr_.getNetworkConfig();
        if (networkConfig.enableKeepAlive) {
            enable_tcp_keepalive();
        }

        //  启动应用层心跳
        if (networkConfig.enableAppHeartbeat) {
            heartbeat_enabled_.store(true);
            heartbeat_interval_ms_.store(networkConfig.heartbeatIntervalMs);
            start_heartbeat_timer();
        }

        do_read();

        // 连接成功后，开始发送队列中的消息
        do_send_from_queue();
    }
    else {
        connection_monitor_->set_state(ConnectionState::Disconnected);
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
            "[IPC] 连接失败: " + SystemErrorUtil::GetErrorMessage(ec));
        try_reconnect();
    }
}

void Lusp_AsioLoopbackIpcClient::handle_read_result(const std::error_code& ec, std::size_t bytes_transferred) {
    if (!ec && bytes_transferred > 0) {
        if (on_message_) {
            std::string message(buffer_->data(), bytes_transferred);
            on_message_(message);

            g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_DEBUG,
                "[IPC] 接收消息，长度: " + std::to_string(bytes_transferred));
        }
        do_read(); // 继续读取
    }
    else {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_WARN,
            "[IPC] 读取失败: " + SystemErrorUtil::GetErrorMessage(ec));
        try_reconnect();
    }
}

// ==================== TCP Keep-Alive 实现 ====================

void Lusp_AsioLoopbackIpcClient::enable_tcp_keepalive() {
    try {
        const auto& networkConfig = config_mgr_.getNetworkConfig();

        // 1️ 启用 Keep-Alive 选项
        asio::socket_base::keep_alive option(true);
        socket_->set_option(option);

#ifdef _WIN32
        // 2️ Windows 平台特定设置
        SOCKET native_socket = socket_->native_handle();

        tcp_keepalive keepalive_vals;
        keepalive_vals.onoff = 1;
        keepalive_vals.keepalivetime = networkConfig.keepAliveIntervalMs;  // 首次探测延迟
        keepalive_vals.keepaliveinterval = 1000;  // 探测间隔 1秒

        DWORD bytes_returned;
        int result = WSAIoctl(native_socket, SIO_KEEPALIVE_VALS,
            &keepalive_vals, sizeof(keepalive_vals),
            nullptr, 0, &bytes_returned, nullptr, nullptr);

        if (result == 0) {
            g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
                "[IPC] TCP Keep-Alive 已启用 (间隔: " +
                std::to_string(networkConfig.keepAliveIntervalMs) + "ms)");
        }
        else {
            g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_WARN,
                "[IPC] 设置 TCP Keep-Alive 失败: " + std::to_string(WSAGetLastError()));
        }
#else
        // 3️ Linux 平台设置
        int fd = socket_->native_handle();

        // 空闲时间（秒）
        int keepalive_time = networkConfig.keepAliveIntervalMs / 1000;
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_time, sizeof(keepalive_time));

        // 探测间隔（秒）
        int keepalive_interval = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_interval, sizeof(keepalive_interval));

        // 探测次数
        int keepalive_count = 3;
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_count, sizeof(keepalive_count));

        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
            "[IPC] TCP Keep-Alive 已启用 (间隔: " +
            std::to_string(networkConfig.keepAliveIntervalMs) + "ms)");
#endif
    }
    catch (const std::exception& e) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
            "[IPC] 设置 TCP Keep-Alive 异常: " + std::string(e.what()));
    }
}

// ==================== 应用层心跳实现 ====================

void Lusp_AsioLoopbackIpcClient::enable_heartbeat(bool enable) {
    heartbeat_enabled_.store(enable);

    if (enable && is_connected()) {
        start_heartbeat_timer();
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
            "[IPC] 应用层心跳已启用 (间隔: " +
            std::to_string(heartbeat_interval_ms_.load()) + "ms, 客户端: " + client_computer_name_ + ")");
    }
    else {
        stop_heartbeat_timer();
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
            "[IPC] 应用层心跳已禁用");
    }
}

void Lusp_AsioLoopbackIpcClient::set_heartbeat_interval(uint32_t interval_ms) {
    heartbeat_interval_ms_.store(interval_ms);
    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
        "[IPC] 心跳间隔已更新: " + std::to_string(interval_ms) + "ms");
}

void Lusp_AsioLoopbackIpcClient::start_heartbeat_timer() {
    if (!heartbeat_timer_) {
        heartbeat_timer_ = std::make_shared<asio::steady_timer>(io_context_);
    }

    auto interval = std::chrono::milliseconds(heartbeat_interval_ms_.load());
    heartbeat_timer_->expires_after(interval);
    heartbeat_timer_->async_wait([this](std::error_code ec) {
        if (!ec && heartbeat_enabled_.load() && is_connected()) {
            send_heartbeat_ping();
            check_heartbeat_timeout();
            start_heartbeat_timer();  // 递归调度
        }
        });
}

void Lusp_AsioLoopbackIpcClient::stop_heartbeat_timer() {
    heartbeat_enabled_.store(false);
    if (heartbeat_timer_) {
        heartbeat_timer_->cancel();
    }
}

void Lusp_AsioLoopbackIpcClient::send_heartbeat_ping() {
    try {
        // 使用 FlatBuffer 构建心跳 PING 消息
        flatbuffers::FlatBufferBuilder builder(256);

        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        auto client_name_str = builder.CreateString(client_computer_name_);
        auto client_version_str = builder.CreateString(config_mgr_.getUploadConfig().clientVersion);
        auto payload_str = builder.CreateString("");  // 可选的附加数据

        uint32_t sequence = heartbeat_sequence_.fetch_add(1);

        auto heartbeat = UploadClient::Sync::CreateFBS_HeartbeatMessage(builder,
            UploadClient::Sync::FBS_HeartbeatType_FBS_HEARTBEAT_PING,
            sequence,
            static_cast<uint64_t>(now_ms),
            client_name_str,
            client_version_str,
            payload_str);

        builder.Finish(heartbeat);

        // 发送心跳消息（前4字节为长度）
        uint32_t msg_size = builder.GetSize();
        std::vector<char> buffer(4 + msg_size);

        // 小端序写入长度
        buffer[0] = static_cast<char>(msg_size & 0xFF);
        buffer[1] = static_cast<char>((msg_size >> 8) & 0xFF);
        buffer[2] = static_cast<char>((msg_size >> 16) & 0xFF);
        buffer[3] = static_cast<char>((msg_size >> 24) & 0xFF);

        // 拷贝心跳数据
        std::memcpy(buffer.data() + 4, builder.GetBufferPointer(), msg_size);

        auto data = std::make_shared<std::vector<char>>(std::move(buffer));

        asio::async_write(*socket_, asio::buffer(*data),
            [this, data, sequence](std::error_code ec, std::size_t bytes_sent) {
                if (!ec) {
                    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_DEBUG,
                        "[IPC] ❤️ 心跳 PING #" + std::to_string(sequence) +
                        " 发送成功 (" + std::to_string(bytes_sent) + " 字节)");
                }
                else {
                    uint32_t failure_count = heartbeat_failure_count_.fetch_add(1);
                    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_WARN,
                        "[IPC] 心跳 PING #" + std::to_string(sequence) +
                        " 发送失败 (连续失败: " + std::to_string(failure_count + 1) + "): " + ec.message());

                    // 心跳发送失败，触发重连
                    const auto& networkConfig = config_mgr_.getNetworkConfig();
                    if (failure_count + 1 >= networkConfig.heartbeatMaxFailures) {
                        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
                            "[IPC] 心跳连续失败 " + std::to_string(failure_count + 1) + " 次，触发重连");
                        try_reconnect();
                    }
                }
            });
    }
    catch (const std::exception& e) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
            "[IPC] 构建心跳消息异常: " + std::string(e.what()));
    }
}

void Lusp_AsioLoopbackIpcClient::handle_heartbeat_pong(const std::string& pong_data) {
    try {
        // 解析 FlatBuffer PONG 消息（跳过前4字节长度前缀）
        const uint8_t* buf = reinterpret_cast<const uint8_t*>(pong_data.data()) + 4;
        auto heartbeat = flatbuffers::GetRoot<UploadClient::Sync::FBS_HeartbeatMessage>(buf);

        if (heartbeat->type() == UploadClient::Sync::FBS_HeartbeatType_FBS_HEARTBEAT_PONG) {
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();

            last_pong_time_ms_.store(now_ms);
            heartbeat_failure_count_.store(0);  // 重置失败计数

            // 计算往返时延（RTT）
            uint64_t rtt = now_ms - heartbeat->timestamp();

            g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_DEBUG,
                "[IPC] 💚 心跳 PONG #" + std::to_string(heartbeat->sequence()) +
                " 收到 (RTT: " + std::to_string(rtt) + "ms)");
        }
    }
    catch (const std::exception& e) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
            "[IPC] 解析心跳 PONG 异常: " + std::string(e.what()));
    }
}

void Lusp_AsioLoopbackIpcClient::check_heartbeat_timeout() {
    const auto& networkConfig = config_mgr_.getNetworkConfig();

    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last_pong = last_pong_time_ms_.load();

    // 如果从未收到过 PONG，跳过检查
    if (last_pong == 0) {
        return;
    }

    // 检查是否超时
    uint64_t elapsed = now_ms - last_pong;
    if (elapsed > networkConfig.heartbeatTimeoutMs) {
        uint32_t failure_count = heartbeat_failure_count_.fetch_add(1);

        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_WARN,
            "[IPC] ⚠️ 心跳超时 #" + std::to_string(failure_count + 1) +
            " (未收到 PONG 超过 " + std::to_string(elapsed) + "ms)");

        // 连续超时，触发重连
        if (failure_count + 1 >= networkConfig.heartbeatMaxFailures) {
            g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
                "[IPC] 💔 心跳连续超时 " + std::to_string(failure_count + 1) + " 次，触发重连");
            disconnect();
            try_reconnect();
        }
    }
}

std::string Lusp_AsioLoopbackIpcClient::get_computer_name() const {
#ifdef _WIN32
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) {
        return std::string(buffer);
    }
    return "Unknown-Windows";
#else
    char buffer[256];
    if (gethostname(buffer, sizeof(buffer)) == 0) {
        return std::string(buffer);
    }
    return "Unknown-Linux";
#endif
}