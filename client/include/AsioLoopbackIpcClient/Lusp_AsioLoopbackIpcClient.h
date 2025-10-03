#ifndef LUSP_ASIO_LOOPBACK_IPC_CLIENT_H
#define LUSP_ASIO_LOOPBACK_IPC_CLIENT_H

#include "asio/asio.hpp"
#include "MessageQueue/PersistentMessageQueue.h"
#include "MessageQueue/ConnectionMonitor.h"
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
     * @param priority 消息优先级(0最高)
     */
    void send(const std::string& message, uint32_t priority = 0);

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

    /**
     * @brief 获取队列统计信息
     */
    PersistentMessageQueue::Statistics get_queue_statistics() const;

private:
    void do_read();
    void do_send_from_queue();  // 从队列发送消息
    void try_reconnect();
    void handle_connect_result(const std::error_code& ec, const asio::ip::tcp::endpoint& endpoint);
    void handle_read_result(const std::error_code& ec, std::size_t bytes_transferred);
    void handle_send_result(const std::error_code& ec, std::size_t bytes_transferred, uint64_t msg_id);

    asio::io_context&                               io_context_;                  ///< Asio IO 上下文
    const ClientConfigManager&                      config_mgr_;                  ///< 客户端配置管理器
    std::shared_ptr<asio::ip::tcp::socket>          socket_;                      ///< TCP socket
    std::shared_ptr<std::vector<char>>              buffer_;                      ///< 读缓冲区
    MessageCallback                                 on_message_;                  ///< 消息接收回调
    std::mutex                                      send_mutex_;                  ///< 发送消息的互斥锁

    // 消息队列和连接监测
    std::unique_ptr<PersistentMessageQueue>         message_queue_;               ///< 持久化消息队列
    std::unique_ptr<ConnectionMonitor>              connection_monitor_;          ///< 连接监测器
    std::atomic<bool>                               is_sending_{ false };         ///< 是否正在发送

    // 重连相关状态
    int                                             current_reconnect_attempts_;  ///< 当前重连尝试次数
    bool                                            is_connecting_;               ///< 是否正在连接
    bool                                            is_permanently_stopped_;      ///< 达到最大重连次数后永久停止
    std::shared_ptr<asio::steady_timer>             reconnect_timer_;             /// 重连定时器
};

#endif // LUSP_ASIO_LOOPBACK_IPC_CLIENT_H
