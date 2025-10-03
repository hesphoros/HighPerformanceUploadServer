#include <WinSock2.h>
#include <Windows.h>
#include <QApplication>
#include "MainWindow.h"
#include "log_headers.h"
#include <UniConv.h>
#include "NotificationService/Lusp_SyncFilesNotificationService.h"
#include "SyncUploadQueue/Lusp_SyncUploadQueuePrivate.h"
#include "SyncUploadQueue/Lusp_SyncUploadQueue.h"
#include "AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.h"
#include <memory>
#include <filesystem>
#include <vector>
#include "Config/ClientConfigManager.h"

int main(int argc, char* argv[]) {
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
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "配置文件验证失败，程序退出");
        for (const auto& err : cfgMgr.getValidationErrors()) {
            g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, " - " + err);
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
        g_luspLogWriteImpl.WriteLogContent(LOG_ERROR, "应用程序异常退出，错误码：" + std::to_string(ret));
    }
    // 确保后台线程安全退出
    notifier->stop();
    notifier.reset();

    // 关闭日志系统
    shutdownLogging();

    return ret;
}
