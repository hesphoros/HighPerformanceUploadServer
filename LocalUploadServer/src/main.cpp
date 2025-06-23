#include "AsioLoopbackIpcServer/Lusp_AsioLoopbackIpcServer.h"
#include "stl_headers.h"

void run_server() {
    asio::io_context io_context;
	Lusp_AsioIpcConfig config;
    Lusp_AsioLoopbackIpcServer server(io_context, config);
    server.start([&server](const std::string& msg, std::shared_ptr<asio::ip::tcp::socket> sock) {
        std::cout << "[Server] Received: " << msg << std::endl;
        // Echo back
        server.broadcast("Echo from server: " + msg);
    });

    io_context.run();
}



int main() {
    // 启动服务器线程
    std::thread server_thread(run_server);

    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    server_thread.join();

    return 0;
}