#include "AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.h"
#include "Config/ClientConfigManager.h"
#include "log_headers.h"
#include "utils/SystemErrorUtil.h"
#include <chrono>
#include <thread>


Lusp_AsioLoopbackIpcClient::Lusp_AsioLoopbackIpcClient(asio::io_context& io_context, const ClientConfigManager& configMgr)
    : io_context_(io_context)
    , config_mgr_(configMgr)
    , socket_(std::make_shared<asio::ip::tcp::socket>(io_context))
    , current_reconnect_attempts_(0)
    , is_connecting_(false)
    , is_permanently_stopped_(false)
    , reconnect_timer_(std::make_shared<asio::steady_timer>(io_context)) {

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

    // 从队列取出消息
    auto ipc_message_opt = message_queue_->dequeue();
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
                handle_send_result(ec, bytes_sent);

                if (!ec) {
                    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_DEBUG,
                        "[IPC] 消息 " + std::to_string(msg_id) + " 发送成功，长度: " + std::to_string(len));
                }
                else {
                    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
                        "[IPC] 消息 " + std::to_string(msg_id) + " 发送失败: " + SystemErrorUtil::GetErrorMessage(ec));
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

void Lusp_AsioLoopbackIpcClient::handle_send_result(const std::error_code& ec, std::size_t bytes_transferred) {
    is_sending_.store(false);

    if (!ec) {
        // 发送成功
        connection_monitor_->record_send_success();

        // 继续发送队列中的下一条消息
        do_send_from_queue();
    }
    else {
        // 发送失败，交给连接监测器判断是否需要重连
        bool need_reconnect = connection_monitor_->record_send_failure(ec);

        if (!need_reconnect) {
            // 不需要重连，继续发送下一条（可能是临时错误）
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
    // 如果启用了退避策略，增加延迟时间
    if (networkConfig.enableReconnectBackoff && current_reconnect_attempts_ > 1) {
        delay = std::min(delay * current_reconnect_attempts_, networkConfig.reconnectBackoffMs);
    }

    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
        "[IPC] 第 " + std::to_string(current_reconnect_attempts_) + " 次重连," +
        std::to_string(delay) + "ms 后尝试");

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