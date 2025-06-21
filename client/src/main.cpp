#include <QApplication>
#include "MainWindow.h"
#include "log_headers.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("Upload Client");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("High Performance Upload");
    initializeLogging();  // 初始化日志系统
    // 创建主窗口
    MainWindow window;
    window.show();
    
    return app.exec();
}
