#include "AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.h"
#include "log_headers.h"



Lusp_AsioLoopbackIpcClient::Lusp_AsioLoopbackIpcClient(asio::io_context& io_context, const Lusp_AsioIpcConfig& config)
    : io_context_(io_context),
    socket_(std::make_shared<asio::ip::tcp::socket>(io_context)),
    config_(config) {
    buffer_ = std::make_shared<std::vector<char>>(config.buffer_size);
}


void Lusp_AsioLoopbackIpcClient::connect()
{
    asio::ip::tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(config_.host, std::to_string(config_.port));
    asio::async_connect(*socket_, endpoints,
        [this](std::error_code ec, asio::ip::tcp::endpoint) {
            if (!ec) {
                do_read();
            }
            else {
                try_reconnect();
            }
        });
}

void Lusp_AsioLoopbackIpcClient::send(const std::string& message)
{
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (socket_ && socket_->is_open()) {
        auto data = std::make_shared<std::string>(message);
        asio::async_write(*socket_, asio::buffer(*data),
            [data](std::error_code, std::size_t) {
                // 可选：写入完成后日志
                std::string msg = "[IPC] Send message  ";
				g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_DEBUG, msg);
            });
    } else {
        g_LogAsioLoopbackIpcClient.WriteLogContent(LOG_ERROR, "[IPC] Socket not connected, send failed.");
    }
}


void Lusp_AsioLoopbackIpcClient::on_receive(MessageCallback cb) {
    on_message_ = cb;
}

void Lusp_AsioLoopbackIpcClient::do_read()
{
    socket_->async_read_some(asio::buffer(*buffer_),
        [this](std::error_code ec, std::size_t len) {
            if (!ec && on_message_) {
                std::string raw(buffer_->data(), len);
                std::string msg = (raw);
                on_message_(msg);
                do_read();
            }
            else {
                try_reconnect();
            }
        });
}

void Lusp_AsioLoopbackIpcClient::try_reconnect()
{
    socket_ = std::make_shared<asio::ip::tcp::socket>(io_context_);
    std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_interval_ms));
    connect();
}