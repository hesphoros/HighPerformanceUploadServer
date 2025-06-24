#include "AsioLoopbackIpcServer/Lusp_AsioLoopbackIpcServer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include "upload_file_info_generated.h"
#include "flatbuffers/flatbuffers.h"
#include "AsioLoopbackIpcServer/Lusp_AsioIpcSender.h"
#include "log_headers.h"

// ---- Server ----
Lusp_AsioLoopbackIpcServer::Lusp_AsioLoopbackIpcServer(asio::io_context& io_context, const Lusp_AsioIpcConfig& config)
    : io_context_(io_context),
      acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), config.port)),
      config_(config) {
    sender_ = std::make_unique<Lusp_AsioIpcSender>(clients_, clients_mutex_);
}

void Lusp_AsioLoopbackIpcServer::start(MessageCallback on_message) {
    on_message_ = on_message;
    do_accept();
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
                on_message_(msg, socket);

                // 移除已处理
                state->buffer.erase(state->buffer.begin(), state->buffer.begin() + 4 + msg_len);
            }
            do_read(socket, state);
        } else {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_.erase(socket);
        }
    });
}

void Lusp_AsioLoopbackIpcServer::broadcast(const std::string& message) {
    sender_->broadcast(message);
}

void Lusp_AsioLoopbackIpcServer::broadcast(const void* data, size_t size) {
    sender_->broadcast(data, size);
}