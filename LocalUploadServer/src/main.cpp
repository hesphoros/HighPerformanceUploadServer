#include "AsioLoopbackIpcServer/Lusp_AsioLoopbackIpcServer.h"
#include "upload_file_info_generated.h"
#include "flatbuffers/flatbuffers.h"
#include "stl_headers.h"
#include <Windows.h>
#include "log_headers.h"
#include "log/LightLogWriteImpl.h"
#include "tabulate/tabulate.hpp"

void on_flatbuffer_message(const void* data, size_t size) {
    flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(data), size);
    if (UploadClient::Sync::VerifyFBS_SyncUploadFileInfoBuffer(verifier)) {
        auto fb_msg = UploadClient::Sync::GetFBS_SyncUploadFileInfo(data);
        auto native_msg = fb_msg->UnPack();
        //std::cout << "[FlatBuffer] file_name: " << native_msg->s_file_full_name_value << std::endl;

        tabulate::Table table;
        table.add_row({
            "ClientDevice", "UploadFileType", "SyncFileFullName", "SyncFileOnlyName",
            "SyncFileSizeVaule", "SyncFileRecordTime", "SyncFileMd5ValueInfo",
            "FileExistPolicyValue", "SyncAuthTokenValue", "SyncUploadTimeStamp",
            "UploadStatusInf", "DescriptionInfo"
        });
        table[0].format()
            .font_style({ tabulate::FontStyle::bold })
            .font_align({ tabulate::FontAlign::center });
        table.add_row({
            native_msg->s_lan_client_device,
            std::to_string(native_msg->e_upload_file_typed),
            LUSP_UNICONV->ToLocaleFromUtf8(native_msg->s_file_full_name_value),
            LUSP_UNICONV->ToLocaleFromUtf8(native_msg->s_only_file_name_value),
            std::to_string(native_msg->s_sync_file_size_value),
            native_msg->s_file_record_time_value,
            native_msg->s_file_md5_value_info,
            std::to_string(native_msg->e_file_exist_policy),
            native_msg->s_auth_token_values,
            std::to_string(native_msg->e_upload_status_inf),
            native_msg->s_description_info,
            native_msg->s_description_info
        });
        std::cout << table << std::endl;
    } else {
        std::cout << "[Callback] 收到未知或非法FlatBuffer消息" << std::endl;
    }
}
void run_server() {
    asio::io_context io_context;
	Lusp_AsioIpcConfig config;
    // 注册回调
    Lusp_AsioLoopbackIpcServer server(io_context, config);
    server.start([](const std::string& data, std::shared_ptr<asio::ip::tcp::socket>) {
        on_flatbuffer_message(data.data(), data.size());
        });
	std::cout << "[LocalUploadServer] Server started and listening on port " << config.port << std::endl;
    io_context.run();
}



int main() {
    
    SetConsoleOutputCP(CP_UTF8);
	initializeLogger();
    // 启动服务器线程
    std::thread server_thread(run_server);
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    server_thread.join();

    return 0;
}