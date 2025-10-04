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
     * @brief 全局配置使用 ClientConfigManager
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

    /**
     * @brief 启用/禁用应用层心跳
     * @param enable 是否启用 默认启用
     */
    void enable_heartbeat(bool enable = true);

    /**
     * @brief 设置心跳间隔
     * @param interval_ms 心跳间隔（毫秒）
     */
    void set_heartbeat_interval(uint32_t interval_ms);

private:

    /**
     * @brief 读取数据
     * @return void
     */
    void do_read();

    /**
     * @brief 从持久化消息队列发送消息
     * @return void
     */
    void do_send_from_queue(); 

    /**
     * @brief 尝试重连
     * @return void
     * @details 使用指数退避算法进行重连
     */
    void try_reconnect();

    /**
     * @brief 处理连接结果
     * @details 处理连接结果 连接成功设置TCP Keep-Alive 以及启动心跳
     * @param ec  错误码
     * @param endpoint  连接的端点
     */
    void handle_connect_result(const std::error_code& ec, const asio::ip::tcp::endpoint& endpoint);
    void handle_read_result(const std::error_code& ec, std::size_t bytes_transferred);
    void handle_send_result(const std::error_code& ec, std::size_t bytes_transferred, uint64_t msg_id);

    // TCP Keep-Alive
    void enable_tcp_keepalive();

    // 应用层心跳
    void start_heartbeat_timer(); // 启动心跳定时器
    void stop_heartbeat_timer();  // 停止心跳定时器
    void send_heartbeat_ping();   // 发送心跳PING
    void handle_heartbeat_pong(const std::string& pong_data); // 处理心跳PONG
    void check_heartbeat_timeout();         // 检查心跳超时
    std::string get_computer_name() const;  // 获取计算机名称

private:
    //-------------------------------------------------------------------------------------------
    // Private Members @{
    //-------------------------------------------------------------------------------------------
    asio::io_context&                               io_context_;                     ///< Asio IO 上下文
    const ClientConfigManager&                      config_mgr_;                     ///< 客户端配置管理器
    std::shared_ptr<asio::ip::tcp::socket>          socket_;                         ///< TCP socket
    std::shared_ptr<std::vector<char>>              buffer_;                         ///< 读缓冲区
    MessageCallback                                 on_message_;                     ///< 消息接收回调
    std::mutex                                      send_mutex_;                     ///< 发送消息的互斥锁

    // 消息队列和连接监测
    std::unique_ptr<PersistentMessageQueue>         message_queue_;                  ///< 持久化消息队列
    std::unique_ptr<ConnectionMonitor>              connection_monitor_;             ///< 连接监测器
    std::atomic<bool>                               is_sending_{ false };            ///< 是否正在发送

    // 重连相关状态
    int                                             current_reconnect_attempts_;     ///< 当前重连尝试次数
    bool                                            is_connecting_;                  ///< 是否正在连接
    bool                                            is_permanently_stopped_;         ///< 达到最大重连次数后永久停止
    std::shared_ptr<asio::steady_timer>             reconnect_timer_;                ///< 重连定时器

    // 心跳相关状态
    std::shared_ptr<asio::steady_timer>             heartbeat_timer_;                ///< 心跳定时器
    std::atomic<bool>                               heartbeat_enabled_{ false };     ///< 是否启用心跳
    std::atomic<uint32_t>                           heartbeat_interval_ms_{ 10000 }; ///< 心跳间隔
    std::atomic<uint32_t>                           heartbeat_sequence_{ 0 };        ///< 心跳序列号
    std::atomic<uint64_t>                           last_pong_time_ms_{ 0 };         ///< 最后收到PONG时间
    std::atomic<uint32_t>                           heartbeat_failure_count_{ 0 };   ///< 心跳失败计数
    std::string                                     client_computer_name_;           ///< 客户端计算机名称
    //-------------------------------------------------------------------------------------------
    // @}
    //-------------------------------------------------------------------------------------------
};

#endif // LUSP_ASIO_LOOPBACK_IPC_CLIENT_H
