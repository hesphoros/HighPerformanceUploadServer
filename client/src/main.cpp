#include <QApplication>
#include "MainWindow.h"
#include "log_headers.h"
#include "log/UniConv.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("SyncFilesUploadClient");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("High Performance Upload");

    // UniConv::GetInstance()->SetDefaultEncoding("UTF-8");
    initializeLogging();  // 初始化日志系统
    // 创建主窗口
    MainWindow window;
    window.show();
    
    return app.exec();
}
