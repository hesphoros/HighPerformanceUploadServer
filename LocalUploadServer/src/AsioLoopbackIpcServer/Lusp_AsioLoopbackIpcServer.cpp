#include "AsioLoopbackIpcServer/Lusp_AsioLoopbackIpcServer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include "upload_file_info_generated.h"
#include "flatbuffers/flatbuffers.h"

// ---- Server ----
Lusp_AsioLoopbackIpcServer::Lusp_AsioLoopbackIpcServer(asio::io_context& io_context, const Lusp_AsioIpcConfig& config)
    : io_context_(io_context),
      acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), config.port)),
      config_(config) {}

void Lusp_AsioLoopbackIpcServer::start(MessageCallback on_message) {
    on_message_ = on_message;
    do_accept();
}

void Lusp_AsioLoopbackIpcServer::do_accept() {
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    acceptor_.async_accept(*socket, [this, socket](std::error_code ec) {
        if (!ec) {
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                clients_.insert(socket);
            }
            do_read(socket);
        }
        do_accept();
    });
}

void Lusp_AsioLoopbackIpcServer::do_read(std::shared_ptr<asio::ip::tcp::socket> socket) {
    auto buffer = std::make_shared<std::vector<char>>(config_.buffer_size);
    socket->async_read_some(asio::buffer(*buffer), [this, socket, buffer](std::error_code ec, std::size_t len) {
        if (!ec && on_message_) {
            // 尝试解析 FlatBuffers 消息
            const void* data = buffer->data();
            size_t size = len;
            flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(data), size);
            if (UploadClient::Sync::VerifyFBS_SyncUploadFileInfoBuffer(verifier)) {
                auto fb_msg = UploadClient::Sync::GetFBS_SyncUploadFileInfo(data);
                auto native_msg = fb_msg->UnPack();
                // 这里可以自定义处理native_msg，例如打印或回调
                std::cout << "[FlatBuffer] file_name: " << native_msg->s_file_full_name_value << std::endl;
                // 你可以扩展on_message_为多态回调，或在此处处理FlatBuffer消息
            } else {
                // 非 FlatBuffers 消息，走原有字符串回调
                std::string raw(buffer->data(), len);
                on_message_(raw, socket);
            }
            do_read(socket);
        } else {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_.erase(socket);
        }
    });
}




void Lusp_AsioLoopbackIpcServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& client : clients_) {
        if (client && client->is_open()) {
          
            auto data = std::make_shared<std::string>(message);
            asio::async_write(*client, asio::buffer(*data),
                [data](std::error_code, std::size_t) {
                 
                });
        }
    }
}