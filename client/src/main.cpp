#include <WinSock2.h>
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <QApplication>
#include "MainWindow.h"
#include "log_headers.h"
#include <UniConv.h>
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


// ===================== 配置管理器测试模块 =====================






void TestDefaultConfigUploadClient() {
    auto& cfgMgr = ClientConfigManager::getInstance();
    cfgMgr.loadFromFile("./config/upload_client.toml");
    if (cfgMgr.validateConfig()) {
        std::cout << "配置文件验证通过。" << std::endl;
    }
    else {
        std::cout << "配置文件验证失败，错误如下：" << std::endl;
        for (const auto& err : cfgMgr.getValidationErrors()) {
            std::cout << " - " << err << std::endl;
        }
    }

    std::string str = cfgMgr.exportToTomlString();
    std::cout << str << std::endl;
}


void SetConsoleToUTF8() {
    // 启用 UTF-8 控制台输出支持
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 启用虚拟终端处理
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    // 设置 C++ iostream 的区域设置
    std::locale::global(std::locale("en_US.UTF-8"));
    std::ios_base::sync_with_stdio(false);
}

int main(int argc, char* argv[]) {


    SetConsoleToUTF8();
    QApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName("SyncFilesUploadClient");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("High Performance Upload");

    initializeLogging();  // 初始化日志系统

    // 加载配置管理器（必须在创建NotificationService之前）
    auto& cfgMgr = ClientConfigManager::getInstance();
    cfgMgr.loadFromFile("./config/upload_client.toml");
    if (!cfgMgr.validateConfig()) {
        std::cerr << "配置文件验证失败，程序退出：" << std::endl;
        for (const auto& err : cfgMgr.getValidationErrors()) {
            std::cerr << " - " << err << std::endl;
        }
        return -1;
    }

    // 用智能指针管理 NotificationService（必须传入配置管理器）
    std::unique_ptr<Lusp_SyncFilesNotificationService> notifier =
        std::make_unique<Lusp_SyncFilesNotificationService>(Lusp_SyncUploadQueue::instance(), cfgMgr);

    notifier->start();

    
    // 创建主窗口
    MainWindow window;
    window.show();
    int ret = app.exec();

    if (ret != 0) {
        std::cerr << "应用程序异常退出，错误码：" << ret << std::endl;
    }
    // 确保后台线程安全退出
    notifier->stop();
    notifier.reset();

    // 关闭日志系统
    shutdownLogging();

    return ret;
}
