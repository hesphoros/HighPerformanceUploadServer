#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("Upload Client");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("High Performance Upload");
    
    // 创建主窗口
    MainWindow window;
    window.show();
    
    return app.exec();
}
