#include <WinSock2.h>
#include <Windows.h>
#include <QApplication>
#include "MainWindow.h"
#include "log_headers.h"
#include "log/UniConv.h"
#include "NotificationService/Lusp_SyncFilesNotificationService.h"
#include "SyncUploadQueue/Lusp_SyncUploadQueuePrivate.h"
#include "SyncUploadQueue/Lusp_SyncUploadQueue.h"
#include "AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.h"
#include <memory>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <vector>

#include "Config/ClientConfigManager.h"


LightLogWrite_Impl configLogWriter;

// ===================== 配置管理器测试模块 =====================






void TestDefaultConfigUploadClient(){
    auto& cfgMgr = ClientConfigManager::getInstance();
    cfgMgr.loadFromFile("./config/upload_client.toml");
    if (cfgMgr.validateConfig()) {
        std::cout << "配置文件验证通过。" << std::endl;
    } else {
        std::cout << "配置文件验证失败，错误如下：" << std::endl;
        for (const auto& err : cfgMgr.getValidationErrors()) {
            std::cout << " - " << err << std::endl;
        }
    }

    std::string str = cfgMgr.exportToTomlString();
    std::cout << str << std::endl;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    SetConsoleOutputCP(CP_UTF8);
    // 设置应用程序信息
    app.setApplicationName("SyncFilesUploadClient");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("High Performance Upload");

    // UniConv::GetInstance()->SetDefaultEncoding("UTF-8");
    initializeLogging();  // 初始化日志系统

    // 用智能指针管理 NotificationService
    std::unique_ptr<Lusp_SyncFilesNotificationService> notifier = 
        std::make_unique<Lusp_SyncFilesNotificationService>(Lusp_SyncUploadQueue::instance());
        
    notifier->start();

    // TestConfig();
    TestDefaultConfigUploadClient();

    //client2.join();
    // 创建主窗口
    MainWindow window;
    window.show();
    //std::thread client1([]() { run_client(1); });
    //std::thread client2([]() { run_client(2); });

    //client1.join();
    int ret = app.exec();

    // 确保后台线程安全退出
    notifier->stop();
    notifier.reset();


    return ret;
}
