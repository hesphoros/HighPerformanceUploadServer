#ifndef LUSP_ASIO_LOOPBACK_IPC_SERVER_H
#define LUSP_ASIO_LOOPBACK_IPC_SERVER_H

#include "asio/asio.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <queue>
#include <atomic>
#include <chrono>
#include "AsioLoopbackIpcServer/Lusp_AsioIpcSender.h"

typedef struct Lusp_AsioIpcConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 9000;
    size_t buffer_size = 1024;   // read buffer size
    int reconnect_interval_ms = 1000; //reconnect interval in milliseconds

    // 心跳配置
    bool enable_heartbeat_check = true;        // 是否启用心跳检测
    uint32_t heartbeat_timeout_ms = 60000;     // 心跳超时时间（默认60秒）
    uint32_t heartbeat_check_interval_ms = 5000; // 心跳检查间隔（默认5秒）
}Lusp_AsioIpcConfig, * PLusp_AsioIpcConfig;

// 客户端心跳信息
struct ClientHeartbeatInfo {
    std::string client_name;                    // 客户端计算机名
    std::string client_version;                 // 客户端版本
    uint64_t last_heartbeat_time_ms = 0;        // 最后一次心跳时间（毫秒）
    uint32_t last_sequence = 0;                 // 最后一次序列号
    uint32_t heartbeat_count = 0;               // 收到的心跳总数
};

// 新增：每个连接的状态，包含接收缓冲区
struct ConnState {
    std::vector<char> buffer;
    ClientHeartbeatInfo heartbeat_info;         // 心跳信息
};

class Lusp_AsioLoopbackIpcServer {
public:
    using MessageCallback = std::function<void(const std::string&, std::shared_ptr<asio::ip::tcp::socket>)>;

    Lusp_AsioLoopbackIpcServer(asio::io_context& io_context, const Lusp_AsioIpcConfig& config);
    ~Lusp_AsioLoopbackIpcServer();

    void start(MessageCallback on_message);
    void broadcast(const std::string& message);
    void broadcast(const void* data, size_t size); // 新增二进制广播

    // 心跳相关
    void enable_heartbeat_check(bool enable = true);
    size_t get_active_clients_count() const;
    std::vector<ClientHeartbeatInfo> get_clients_heartbeat_info() const;

private:
    void do_accept();
    // 修改：do_read带ConnState
    void do_read(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<ConnState> state);

    // 心跳相关私有方法
    void handle_heartbeat_ping(const std::string& ping_data, std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<ConnState> state);
    void send_heartbeat_pong(uint32_t sequence, uint64_t timestamp, const std::string& client_name, const std::string& client_version, std::shared_ptr<asio::ip::tcp::socket> socket);
    void start_heartbeat_checker();
    void check_clients_heartbeat();
    uint64_t get_current_time_ms() const;

    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    MessageCallback on_message_;
    std::unordered_set<std::shared_ptr<asio::ip::tcp::socket>> clients_;
    mutable std::mutex clients_mutex_;  // mutable 允许在 const 函数中加锁
    Lusp_AsioIpcConfig config_;
    std::unique_ptr<Lusp_AsioIpcSender> sender_; // 新增

    // 心跳检测相关
    std::atomic<bool> heartbeat_check_enabled_{ true };
    std::shared_ptr<asio::steady_timer> heartbeat_checker_timer_;
    std::unordered_map<std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<ConnState>> socket_states_;
    mutable std::mutex states_mutex_;  // mutable 允许在 const 函数中加锁
};


#endif // LUSP_ASIO_LOOPBACK_IPC_SERVER_H