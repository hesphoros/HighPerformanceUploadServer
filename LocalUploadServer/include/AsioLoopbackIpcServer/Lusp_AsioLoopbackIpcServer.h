#ifndef LUSP_ASIO_LOOPBACK_IPC_SERVER_H
#define LUSP_ASIO_LOOPBACK_IPC_SERVER_H

#include "asio/asio.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <queue>
#include "AsioLoopbackIpcServer/Lusp_AsioIpcSender.h"

typedef struct Lusp_AsioIpcConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 9000;
    size_t buffer_size = 1024;   // read buffer size
    int reconnect_interval_ms = 1000; //reconnect interval in milliseconds
}Lusp_AsioIpcConfig,*PLusp_AsioIpcConfig;

// 新增：每个连接的状态，包含接收缓冲区
struct ConnState {
    std::vector<char> buffer;
};

class Lusp_AsioLoopbackIpcServer {
public:
    using MessageCallback = std::function<void(const std::string&, std::shared_ptr<asio::ip::tcp::socket>)>;

    Lusp_AsioLoopbackIpcServer(asio::io_context& io_context, const Lusp_AsioIpcConfig& config);
    void start(MessageCallback on_message);
    void broadcast(const std::string& message);
    void broadcast(const void* data, size_t size); // 新增二进制广播

private:
    void do_accept();
    // 修改：do_read带ConnState
    void do_read(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<ConnState> state);

    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    MessageCallback on_message_;
    std::unordered_set<std::shared_ptr<asio::ip::tcp::socket>> clients_;
    std::mutex clients_mutex_;
    Lusp_AsioIpcConfig config_;
    std::unique_ptr<Lusp_AsioIpcSender> sender_; // 新增
};


#endif // LUSP_ASIO_LOOPBACK_IPC_SERVER_H