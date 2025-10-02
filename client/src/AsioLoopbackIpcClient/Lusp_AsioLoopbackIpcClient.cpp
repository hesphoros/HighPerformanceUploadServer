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
    , should_reconnect_(true)
    , reconnect_timer_(std::make_shared<asio::steady_timer>(io_context)) {

    const auto& networkConfig = config_mgr_.getNetworkConfig();
    buffer_ = std::make_shared<std::vector<char>>(networkConfig.bufferSize);

    const auto& uploadConfig = config_mgr_.getUploadConfig();
    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
        "[IPC] 初始化客户端: " + uploadConfig.serverHost + ":" + std::to_string(uploadConfig.serverPort));
}


void Lusp_AsioLoopbackIpcClient::connect() {
    if (is_connecting_) {
        return;
    }

    is_connecting_ = true;
    current_reconnect_attempts_ = 0;

    try {
        const auto& uploadConfig = config_mgr_.getUploadConfig();

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
        try_reconnect();
    }
}

void Lusp_AsioLoopbackIpcClient::send(const std::string& message) {
    std::lock_guard<std::mutex> lock(send_mutex_);

    if (!is_connected()) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
            "[IPC] 未连接，发送失败，消息长度: " + std::to_string(message.size()));
        return;
    }

    try {
        // 计算长度并转为4字节（小端序）
        uint32_t len = static_cast<uint32_t>(message.size());
        std::vector<char> buffer(4 + message.size());
        buffer[0] = static_cast<char>(len & 0xFF);
        buffer[1] = static_cast<char>((len >> 8) & 0xFF);
        buffer[2] = static_cast<char>((len >> 16) & 0xFF);
        buffer[3] = static_cast<char>((len >> 24) & 0xFF);

        // 拷贝消息体
        std::memcpy(buffer.data() + 4, message.data(), message.size());

        auto data = std::make_shared<std::vector<char>>(std::move(buffer));
        asio::async_write(*socket_, asio::buffer(*data),
            [this, data, len](std::error_code ec, std::size_t bytes_sent) {
                if (!ec) {

                    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_DEBUG,
                        "[IPC] 消息发送成功，长度: " + std::to_string(len));
                }
                else {
                    g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
                        "[IPC] 消息发送失败: " + SystemErrorUtil::GetErrorMessage(ec));
                }
            });
    }
    catch (const std::exception& e) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
            "[IPC] 发送异常: " + std::string(e.what()));
    }
}

void Lusp_AsioLoopbackIpcClient::on_receive(MessageCallback cb) {
    on_message_ = std::move(cb);
}

void Lusp_AsioLoopbackIpcClient::disconnect() {
    should_reconnect_ = false;

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

    if (!should_reconnect_ || !networkConfig.enableAutoReconnect) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO, "[IPC] 自动重连已禁用");
        return;
    }

    if (current_reconnect_attempts_ >= static_cast<int>(networkConfig.maxReconnectAttempts)) {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR,
            "[IPC] 达到最大重连次数 " + std::to_string(networkConfig.maxReconnectAttempts) + "，停止重连");
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
        "[IPC] 第 " + std::to_string(current_reconnect_attempts_) + " 次重连，" +
        std::to_string(delay) + "ms 后尝试");

    // 使用异步定时器进行重连
    reconnect_timer_->expires_after(std::chrono::milliseconds(delay));
    reconnect_timer_->async_wait([this](std::error_code ec) {
        if (!ec && should_reconnect_) {
            connect();
        }
        });
}

void Lusp_AsioLoopbackIpcClient::handle_connect_result(const std::error_code& ec, const asio::ip::tcp::endpoint& endpoint) {
    is_connecting_ = false;

    if (!ec) {
        current_reconnect_attempts_ = 0; // 重置重连计数
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_INFO,
            "[IPC] 连接成功: " + endpoint.address().to_string() + ":" + std::to_string(endpoint.port()));
        do_read();
    }
    else {
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