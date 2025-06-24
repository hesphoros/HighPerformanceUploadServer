#include "AsioLoopbackIpcServer/Lusp_AsioIpcSender.h"

void Lusp_AsioIpcSender::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& client : clients_) {
        if (client && client->is_open()) {
            auto data = std::make_shared<std::string>(message);
            asio::async_write(*client, asio::buffer(*data),
                [data](std::error_code, std::size_t) {});
        }
    }
}

void Lusp_AsioIpcSender::broadcast(const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& client : clients_) {
        if (client && client->is_open()) {
            auto buf = std::make_shared<std::vector<char>>((char*)data, (char*)data + size);
            asio::async_write(*client, asio::buffer(*buf),
                [buf](std::error_code, std::size_t) {});
        }
    }
}
