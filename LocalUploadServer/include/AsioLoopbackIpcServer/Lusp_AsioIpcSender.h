#pragma once
#include <memory>
#include <string>
#include <unordered_set>
#include <mutex>
#include "asio/asio.hpp"

class Lusp_AsioIpcSender {
public:
    Lusp_AsioIpcSender(std::unordered_set<std::shared_ptr<asio::ip::tcp::socket>>& clients, std::mutex& mutex)
        : clients_(clients), clients_mutex_(mutex) {}

    void broadcast(const std::string& message);
    void broadcast(const void* data, size_t size);

private:
    std::unordered_set<std::shared_ptr<asio::ip::tcp::socket>>& clients_;
    std::mutex& clients_mutex_;
};
