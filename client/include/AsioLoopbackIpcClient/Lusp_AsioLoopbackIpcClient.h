#ifndef LUSP_ASIO_LOOPBACK_IPC_CLIENT_H
#define LUSP_ASIO_LOOPBACK_IPC_CLIENT_H

#include "asio/asio.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#include <mutex>

namespace Lusp_ProtobufUtils {
	class ProtobufMessage;
}

typedef struct Lusp_AsioIpcConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 9000;
	size_t buffer_size = 1024;   // read buffer size
	int reconnect_interval_ms = 1000; //reconnect interval in milliseconds
}Lusp_AsioIpcConfig,*PLusp_AsioIpcConfig;


class Lusp_AsioLoopbackIpcClient {
public:
    using MessageCallback = std::function<void(const std::string&)>;

    Lusp_AsioLoopbackIpcClient(asio::io_context& io_context, const Lusp_AsioIpcConfig& config);
    void connect();
    void send(const std::string& message);
    void on_receive(MessageCallback cb);
private:
    void do_read();
    void try_reconnect();

    asio::io_context&                               io_context_;
    std::shared_ptr<asio::ip::tcp::socket>          socket_;
    std::shared_ptr<std::vector<char>>              buffer_;    
    MessageCallback                                 on_message_;
    std::mutex                                      send_mutex_;
    Lusp_AsioIpcConfig                              config_;
};

#endif // LUSP_ASIO_LOOPBACK_IPC_CLIENT_H
