#include "MainWindow.h"
#include "FileListWidget.h"
#include "SyncUploadQueue/Lusp_SyncUploadQueue.h"  
#include "log_headers.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QStatusBar>
#include <QMenuBar>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_buttonLayout(nullptr)
    , m_fileListWidget(nullptr)
    , m_selectFilesBtn(nullptr)
    , m_uploadBtn(nullptr)
    , m_clearListBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_overallProgressBar(nullptr) {
    
    setupUI();
    connectSignals();
    setupUploadQueue();  // 设置极简上传队列
    
    // 设置窗口属性
    setWindowTitle("高性能文件上传客户端 - 极简架构");
    setMinimumSize(800, 600);
    resize(1000, 700);
    
    // 启用拖拽
    setAcceptDrops(true);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI() {
    // 创建中央widget
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    // 创建主布局
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    
    // 创建文件列表widget
    m_fileListWidget = new FileListWidget(this);
    m_mainLayout->addWidget(m_fileListWidget);
    
    // 创建按钮布局
    m_buttonLayout = new QHBoxLayout();
    
    m_selectFilesBtn = new QPushButton("选择文件", this);
    m_uploadBtn = new QPushButton("开始上传", this);
    m_clearListBtn = new QPushButton("清空列表", this);
    
    m_selectFilesBtn->setMinimumHeight(35);
    m_uploadBtn->setMinimumHeight(35);
    m_clearListBtn->setMinimumHeight(35);
    
    m_buttonLayout->addWidget(m_selectFilesBtn);
    m_buttonLayout->addWidget(m_uploadBtn);
    m_buttonLayout->addWidget(m_clearListBtn);
    m_buttonLayout->addStretch();
    
    m_mainLayout->addLayout(m_buttonLayout);
    
    // 创建状态栏组件
    m_statusLabel = new QLabel("就绪", this);
    m_overallProgressBar = new QProgressBar(this);
    m_overallProgressBar->setVisible(false);
    
    // 添加到状态栏
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_overallProgressBar);
    
    // 创建菜单栏
    auto* fileMenu = menuBar()->addMenu("文件");
    fileMenu->addAction("选择文件", this, &MainWindow::onSelectFilesClicked);
    fileMenu->addSeparator();
    fileMenu->addAction("退出", this, &QWidget::close);
      auto* helpMenu = menuBar()->addMenu("帮助");
    helpMenu->addAction("关于", [this]() {
        QMessageBox::about(this, "关于", 
            "🚀 高性能文件上传客户端 v1.0.0\n\n"
            "🎯 极简架构设计：\n"
            "• UI线程极简：只需 queue.push() 即可\n"
            "• 通知线程独立：条件变量精确唤醒\n"
            "• 行级锁队列：标准C++实现，高并发\n"
            "• 本地服务分离：真实上传由后台处理\n\n"
            "💡 使用方法：\n"
            "拖拽文件到窗口或点击选择文件，系统会自动处理所有上传逻辑！");
    });
    
    helpMenu->addAction("架构说明", [this]() {
        QMessageBox::information(this, "极简架构", 
            "🏗️ 基于 client.md 的极简分层架构：\n\n"
            "📱 UI层：检测文件 → queue.push() → 完事\n"
            "🧵 通知层：条件变量等待 → 给本地服务发通知\n"
            "🏢 服务层：接收通知 → 后台上传 → 进度回调\n\n"
            "✨ 核心优势：\n"
            "• UI响应迅速，不阻塞\n"
            "• 通知线程独立工作\n"
            "• 本地服务处理重活\n"
            "• 职责分离，易维护");
    });
}

void MainWindow::connectSignals() {
    // 按钮信号连接
    connect(m_selectFilesBtn, &QPushButton::clicked, this, &MainWindow::onSelectFilesClicked);
    connect(m_uploadBtn, &QPushButton::clicked, this, &MainWindow::onUploadClicked);
    connect(m_clearListBtn, &QPushButton::clicked, this, &MainWindow::onClearListClicked);
    
    // 文件列表信号连接
    connect(m_fileListWidget, &FileListWidget::filesAdded, this, [this](const QStringList& files) {

        m_statusLabel->setText(QString("已添加 %1 个文件").arg(files.size()));
    });
}

void MainWindow::setupUploadQueue() {
    // 🎯 设置极简上传队列的进度回调（可选）
    Lusp_SyncUploadQueue::instance().setProgressCallback(
        [this](const std::string& filePath, int percentage, const std::string& status) {
            // 线程安全的UI更新
            QMetaObject::invokeMethod(this, [this, filePath, percentage, status]() {
                QString fileName = QFileInfo(QString::fromStdString(filePath)).fileName();
                m_overallProgressBar->setValue(percentage);
                m_statusLabel->setText(QString("上传中: %1 (%2%) - %3")
                    .arg(fileName)
                    .arg(percentage)
                    .arg(QString::fromStdString(status)));
                m_overallProgressBar->setVisible(true);
            });
        }
    );
    
    // 🎯 设置完成回调（可选）
    Lusp_SyncUploadQueue::instance().setCompletedCallback(
        [this](const std::string& filePath, bool success, const std::string& message) {
            QMetaObject::invokeMethod(this, [this, filePath, success, message]() {
                QString fileName = QFileInfo(QString::fromStdString(filePath)).fileName();
                if (success) {
                    m_statusLabel->setText(QString("✅ 上传完成: %1").arg(fileName));
                } else {
                    m_statusLabel->setText(QString("❌ 上传失败: %1 - %2")
                        .arg(fileName).arg(QString::fromStdString(message)));
                }
                
                // 检查队列是否为空
                if (Lusp_SyncUploadQueue::instance().empty()) {
                    m_overallProgressBar->setVisible(false);
                    m_statusLabel->setText("就绪");
                }
            });
        }
    );
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    QStringList filePaths;
    
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            filePaths << url.toLocalFile();
        }
    }
    
    if (!filePaths.isEmpty()) {

        
        addFilesToUploadQueue(filePaths);
        addFilesToList(filePaths);
        
    }
    
    event->acceptProposedAction();
}

void MainWindow::onSelectFilesClicked() {
    QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        "选择要上传的文件",
        "",
        "所有文件 (*)"
    );
    if (!filePaths.isEmpty()) {
        // 日志：打印所有用户选择的文件路径
        for (const QString& filePath : filePaths) {
            g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "用户选择文件: " + filePath.toStdString());
        }
        addFilesToUploadQueue(filePaths);
        addFilesToList(filePaths);
    }
}

void MainWindow::onUploadClicked() {
    QStringList filePaths = m_fileListWidget->getFilePaths();
    
    if (filePaths.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择要上传的文件");
        return;
    }
    
    // 🎯 极简调用：UI只需把文件丢进队列就完事！
    addFilesToUploadQueue(filePaths);
    
    m_statusLabel->setText("文件已提交上传，正在处理...");
}

void MainWindow::addFilesToUploadQueue(const QStringList& filePaths) {
    // 🎯 核心实现：UI线程极简调用 - 只需把文件路径丢进队列
    std::vector<std::string> stdFilePaths;
    
    for (const QString& filePath : filePaths) {
        QFileInfo fileInfo(filePath);
        if (fileInfo.exists() && fileInfo.isFile()) {
            stdFilePaths.push_back(filePath.toStdString());
            // 日志：每个即将入队的文件
            g_luspLogWriteImpl.WriteLogContent(LOG_DEBUG, "准备入队文件: " + filePath.toStdString());
        }
    }
    
    if (!stdFilePaths.empty()) {
        g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "批量入队文件数: " + std::to_string(stdFilePaths.size()));
        // 🎯 这就是全部！UI线程只需要这一行代码！
        // 剩下的全部由通知线程和本地服务自动处理
        Lusp_SyncUploadQueue::instance().push(stdFilePaths);
        g_luspLogWriteImpl.WriteLogContent(LOG_INFO, "已提交到上传队列，总数: " + std::to_string(stdFilePaths.size()));
        m_statusLabel->setText(QString("已提交 %1 个文件到上传队列").arg(stdFilePaths.size()));
    }
}

void MainWindow::onClearListClicked() {
    m_fileListWidget->clearFiles();
    m_statusLabel->setText("已清空文件列表");
}

void MainWindow::addFilesToList(const QStringList& filePaths) {
    m_fileListWidget->addFiles(filePaths);
}
