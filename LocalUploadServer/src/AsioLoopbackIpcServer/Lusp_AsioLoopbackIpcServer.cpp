#include "AsioLoopbackIpcServer/Lusp_AsioLoopbackIpcServer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include "upload_file_info_generated.h"
#include "flatbuffers/flatbuffers.h"
#include "AsioLoopbackIpcServer/Lusp_AsioIpcSender.h"
#include "log_headers.h"

using namespace UploadClient::Sync;

// ---- Server ----
Lusp_AsioLoopbackIpcServer::Lusp_AsioLoopbackIpcServer(asio::io_context& io_context, const Lusp_AsioIpcConfig& config)
    : io_context_(io_context),
    acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), config.port)),
    config_(config),
    heartbeat_check_enabled_(config.enable_heartbeat_check) {
    sender_ = std::make_unique<Lusp_AsioIpcSender>(clients_, clients_mutex_);

    // 初始化心跳检测定时器
    if (config_.enable_heartbeat_check) {
        heartbeat_checker_timer_ = std::make_shared<asio::steady_timer>(io_context_);
    }
}

Lusp_AsioLoopbackIpcServer::~Lusp_AsioLoopbackIpcServer() {
    if (heartbeat_checker_timer_) {
        heartbeat_checker_timer_->cancel();
    }
}

void Lusp_AsioLoopbackIpcServer::start(MessageCallback on_message) {
    on_message_ = on_message;
    do_accept();

    // 启动心跳检测
    if (config_.enable_heartbeat_check) {
        start_heartbeat_checker();
        g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_INFO,
            "Server heartbeat checker started (timeout: " + std::to_string(config_.heartbeat_timeout_ms) + "ms)");
    }

    std::cout << "[Server] Listening on port " << config_.port << std::endl;
    g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_INFO, "Server started and listening on port " + std::to_string(config_.port));
}

void Lusp_AsioLoopbackIpcServer::do_accept() {
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    auto state = std::make_shared<ConnState>();
    acceptor_.async_accept(*socket, [this, socket, state](std::error_code ec) {
        if (!ec) {
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                clients_.insert(socket);
            }
            {
                std::lock_guard<std::mutex> lock(states_mutex_);
                socket_states_[socket] = state;
            }

            // 初始化心跳信息
            state->heartbeat_info.last_heartbeat_time_ms = get_current_time_ms();

            g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_INFO,
                "New client connected. Total clients: " + std::to_string(clients_.size()));

            do_read(socket, state);
        }
        do_accept();
        });
}

void Lusp_AsioLoopbackIpcServer::do_read(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<ConnState> state) {
    auto temp_buffer = std::make_shared<std::vector<char>>(config_.buffer_size);
    socket->async_read_some(asio::buffer(*temp_buffer), [this, socket, state, temp_buffer](std::error_code ec, std::size_t len) {
        if (!ec && on_message_) {
            // 追加到状态缓冲区
            state->buffer.insert(state->buffer.end(), temp_buffer->begin(), temp_buffer->begin() + len);

            // 拆包循环
            while (state->buffer.size() >= 4) {
                uint32_t msg_len = 0;
                msg_len |= (uint8_t)state->buffer[0];
                msg_len |= ((uint8_t)state->buffer[1] << 8);
                msg_len |= ((uint8_t)state->buffer[2] << 16);
                msg_len |= ((uint8_t)state->buffer[3] << 24);
                if (state->buffer.size() < 4 + msg_len) break; // 数据不够

                // 取出完整消息
                std::string msg(state->buffer.begin() + 4, state->buffer.begin() + 4 + msg_len);

                // 尝试解析为心跳消息
                try {
                    auto heartbeat_msg = flatbuffers::GetRoot<FBS_HeartbeatMessage>(
                        reinterpret_cast<const uint8_t*>(msg.data()));

                    if (heartbeat_msg && heartbeat_msg->type() == FBS_HeartbeatType_FBS_HEARTBEAT_PING) {
                        // 处理心跳 PING
                        handle_heartbeat_ping(msg, socket, state);
                    }
                    else {
                        // 普通消息，调用回调
                        on_message_(msg, socket);
                    }
                }
                catch (...) {
                    // 如果不是心跳消息，当作普通消息处理
                    on_message_(msg, socket);
                }

                // 移除已处理
                state->buffer.erase(state->buffer.begin(), state->buffer.begin() + 4 + msg_len);
            }
            do_read(socket, state);
        }
        else {
            std::lock_guard<std::mutex> lock1(clients_mutex_);
            std::lock_guard<std::mutex> lock2(states_mutex_);
            clients_.erase(socket);
            socket_states_.erase(socket);

            g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_INFO,
                "Client disconnected. Remaining clients: " + std::to_string(clients_.size()));
        }
        });
}

void Lusp_AsioLoopbackIpcServer::broadcast(const std::string& message) {
    sender_->broadcast(message);
}

void Lusp_AsioLoopbackIpcServer::broadcast(const void* data, size_t size) {
    sender_->broadcast(data, size);
}

// ===================== 心跳相关实现 =====================

uint64_t Lusp_AsioLoopbackIpcServer::get_current_time_ms() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void Lusp_AsioLoopbackIpcServer::handle_heartbeat_ping(
    const std::string& ping_data,
    std::shared_ptr<asio::ip::tcp::socket> socket,
    std::shared_ptr<ConnState> state) {

    try {
        auto ping_msg = flatbuffers::GetRoot<FBS_HeartbeatMessage>(
            reinterpret_cast<const uint8_t*>(ping_data.data()));

        if (!ping_msg) return;

        // 更新心跳信息
        state->heartbeat_info.last_heartbeat_time_ms = get_current_time_ms();
        state->heartbeat_info.last_sequence = ping_msg->sequence();
        state->heartbeat_info.heartbeat_count++;

        if (ping_msg->client_name()) {
            state->heartbeat_info.client_name = ping_msg->client_name()->str();
        }
        if (ping_msg->client_version()) {
            state->heartbeat_info.client_version = ping_msg->client_version()->str();
        }

        // 发送 PONG 响应
        send_heartbeat_pong(
            ping_msg->sequence(),
            ping_msg->timestamp(),
            state->heartbeat_info.client_name,
            state->heartbeat_info.client_version,
            socket
        );

        // 记录日志
        g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_DEBUG,
            "[HEARTBEAT] Received PING #" + std::to_string(ping_msg->sequence()) +
            " from " + state->heartbeat_info.client_name +
            " (total: " + std::to_string(state->heartbeat_info.heartbeat_count) + ")");

    }
    catch (const std::exception& e) {
        g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_ERROR,
            "Failed to handle heartbeat PING: " + std::string(e.what()));
    }
}

void Lusp_AsioLoopbackIpcServer::send_heartbeat_pong(
    uint32_t sequence,
    uint64_t timestamp,
    const std::string& client_name,
    const std::string& client_version,
    std::shared_ptr<asio::ip::tcp::socket> socket) {

    try {
        flatbuffers::FlatBufferBuilder builder(256);

        auto pong = CreateFBS_HeartbeatMessage(builder,
            FBS_HeartbeatType_FBS_HEARTBEAT_PONG,
            sequence,
            timestamp,
            builder.CreateString(client_name),
            builder.CreateString(client_version),
            builder.CreateString("")
        );

        builder.Finish(pong);

        // 准备发送数据（4字节长度前缀 + FlatBuffer数据）
        uint32_t msg_len = builder.GetSize();
        std::vector<uint8_t> send_buffer(4 + msg_len);

        send_buffer[0] = msg_len & 0xFF;
        send_buffer[1] = (msg_len >> 8) & 0xFF;
        send_buffer[2] = (msg_len >> 16) & 0xFF;
        send_buffer[3] = (msg_len >> 24) & 0xFF;

        memcpy(send_buffer.data() + 4, builder.GetBufferPointer(), msg_len);

        // 异步发送
        asio::async_write(*socket, asio::buffer(send_buffer),
            [this, sequence](std::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_DEBUG,
                        "[HEARTBEAT] Sent PONG #" + std::to_string(sequence));
                }
                else {
                    g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_ERROR,
                        "Failed to send PONG #" + std::to_string(sequence) + ": " + ec.message());
                }
            });

    }
    catch (const std::exception& e) {
        g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_ERROR,
            "Failed to send heartbeat PONG: " + std::string(e.what()));
    }
}

void Lusp_AsioLoopbackIpcServer::start_heartbeat_checker() {
    if (!heartbeat_checker_timer_ || !heartbeat_check_enabled_) {
        return;
    }

    heartbeat_checker_timer_->expires_after(
        std::chrono::milliseconds(config_.heartbeat_check_interval_ms));

    heartbeat_checker_timer_->async_wait([this](std::error_code ec) {
        if (!ec && heartbeat_check_enabled_) {
            check_clients_heartbeat();
            start_heartbeat_checker();  // 继续下一次检查
        }
        });
}

void Lusp_AsioLoopbackIpcServer::check_clients_heartbeat() {
    uint64_t current_time = get_current_time_ms();
    std::vector<std::shared_ptr<asio::ip::tcp::socket>> timeout_clients;

    {
        std::lock_guard<std::mutex> lock(states_mutex_);

        for (const auto& [socket, state] : socket_states_) {
            uint64_t elapsed = current_time - state->heartbeat_info.last_heartbeat_time_ms;

            if (elapsed > config_.heartbeat_timeout_ms) {
                timeout_clients.push_back(socket);

                g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_WARN,
                    "[HEARTBEAT] Client timeout: " + state->heartbeat_info.client_name +
                    " (last seen: " + std::to_string(elapsed) + "ms ago)");
            }
        }
    }

    // 断开超时的客户端
    if (!timeout_clients.empty()) {
        std::lock_guard<std::mutex> lock1(clients_mutex_);
        std::lock_guard<std::mutex> lock2(states_mutex_);

        for (auto& socket : timeout_clients) {
            try {
                socket->close();
            }
            catch (...) {}

            clients_.erase(socket);
            socket_states_.erase(socket);
        }

        g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_INFO,
            "Disconnected " + std::to_string(timeout_clients.size()) +
            " timeout clients. Remaining: " + std::to_string(clients_.size()));
    }
}

void Lusp_AsioLoopbackIpcServer::enable_heartbeat_check(bool enable) {
    heartbeat_check_enabled_ = enable;

    if (enable && heartbeat_checker_timer_) {
        start_heartbeat_checker();
        g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_INFO, "Heartbeat check enabled");
    }
    else {
        if (heartbeat_checker_timer_) {
            heartbeat_checker_timer_->cancel();
        }
        g_LogAsioLoopbackIpcServer.WriteLogContent(LOG_INFO, "Heartbeat check disabled");
    }
}

size_t Lusp_AsioLoopbackIpcServer::get_active_clients_count() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return clients_.size();
}

std::vector<ClientHeartbeatInfo> Lusp_AsioLoopbackIpcServer::get_clients_heartbeat_info() const {
    std::vector<ClientHeartbeatInfo> result;

    std::lock_guard<std::mutex> lock(states_mutex_);
    for (const auto& [socket, state] : socket_states_) {
        result.push_back(state->heartbeat_info);
    }

    return result;
}