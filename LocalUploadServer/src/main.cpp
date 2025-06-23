#include "AsioLoopbackIpcServer/Lusp_AsioLoopbackIpcServer.h"
#include "upload_file_info_generated.h"
#include "flatbuffers/flatbuffers.h"
#include "stl_headers.h"
#include <Windows.h>

void run_server() {
    asio::io_context io_context;
	Lusp_AsioIpcConfig config;
    Lusp_AsioLoopbackIpcServer server(io_context, config);
    server.start([&server](const std::string& msg, std::shared_ptr<asio::ip::tcp::socket> sock) {
        // 判断是否为 FlatBuffers 消息
        flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
        if (UploadClient::Sync::VerifyFBS_SyncUploadFileInfoBuffer(verifier)) {
            auto fb_msg = UploadClient::Sync::GetFBS_SyncUploadFileInfo(msg.data());
            auto native_msg = fb_msg->UnPack();
            std::cout << "[Server][FlatBuffer] file_name: " << native_msg->s_file_full_name_value << std::endl;
            // 可根据需要处理更多字段
        } else {
            std::cout << "[Server] Received: " << msg << std::endl;
            // Echo back
            server.broadcast("Echo from server: " + msg);
        }
    });

    io_context.run();
}



int main() {
    SetConsoleOutputCP(CP_UTF8);
    // 启动服务器线程
    std::thread server_thread(run_server);

    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    server_thread.join();

    return 0;
}