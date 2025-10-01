#ifndef LUSP_ASIO_LOOPBACK_IPC_CLIENT_H
#define LUSP_ASIO_LOOPBACK_IPC_CLIENT_H

#include "asio/asio.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

// 前向声明，避免循环依赖
class ClientConfigManager;

namespace Lusp_ProtobufUtils {
    class ProtobufMessage;
}

/**
 * @brief 基于 Asio 的回环 IPC 客户端
 *
 * 使用 ClientConfigManager 进行统一配置管理
 *
 * 功能特性：
 * - 异步TCP连接和通信
 * - 基于ClientConfigManager的自动重连机制
 * - 线程安全的消息发送
 * - 可配置的缓冲区和超时参数
 * - 完整的网络配置集成
 */
class Lusp_AsioLoopbackIpcClient {
public:
    using MessageCallback = std::function<void(const std::string&)>;

    /**
     * @brief 构造函数 - 使用 ClientConfigManager
     * @param io_context Asio IO 上下文
     * @param configMgr 客户端配置管理器
     */
    Lusp_AsioLoopbackIpcClient(asio::io_context& io_context, const ClientConfigManager& configMgr);

    /**
     * @brief 析构函数
     */
    ~Lusp_AsioLoopbackIpcClient() = default;

    /**
     * @brief 连接到服务器
     */
    void connect();

    /**
     * @brief 发送消息
     * @param message 要发送的消息
     */
    void send(const std::string& message);

    /**
     * @brief 设置消息接收回调
     * @param cb 消息接收回调函数
     */
    void on_receive(MessageCallback cb);

    /**
     * @brief 断开连接
     */
    void disconnect();

    /**
     * @brief 检查连接状态
     * @return 连接状态
     */
    bool is_connected() const;

private:
    void do_read();
    void try_reconnect();
    void handle_connect_result(const std::error_code& ec, const asio::ip::tcp::endpoint& endpoint);
    void handle_read_result(const std::error_code& ec, std::size_t bytes_transferred);

    asio::io_context& io_context_;
    const ClientConfigManager& config_mgr_;
    std::shared_ptr<asio::ip::tcp::socket>          socket_;
    std::shared_ptr<std::vector<char>>              buffer_;
    MessageCallback                                 on_message_;
    std::mutex                                      send_mutex_;

    // 重连相关状态
    int                                             current_reconnect_attempts_;
    bool                                            is_connecting_;
    bool                                            should_reconnect_;
    std::shared_ptr<asio::steady_timer>            reconnect_timer_;
};

#endif // LUSP_ASIO_LOOPBACK_IPC_CLIENT_H
