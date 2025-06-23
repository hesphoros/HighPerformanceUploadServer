#include "FileListWidget.h"
#include "FileInfoQtAdapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QListWidget>
#include <QListWidgetItem>
#include <QFileInfo>
#include <QFont>

// FileItemWidget 实现
FileItemWidget::FileItemWidget(const FileInfoQtAdapter& fileInfo, QWidget* parent)
    : QWidget(parent)
    , m_fileInfo(fileInfo)
    , m_layout(nullptr)
    , m_fileNameLabel(nullptr)
    , m_fileSizeLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr) {
    
    setupUI();
}

void FileItemWidget::setupUI() {
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(10, 5, 10, 5);    // 文件名标签
    m_fileNameLabel = new QLabel(m_fileInfo.getFileName(), this);
    m_fileNameLabel->setMinimumWidth(200);
    QFont font = m_fileNameLabel->font();
    font.setBold(true);
    m_fileNameLabel->setFont(font);
    
    // 文件大小标签
    QString sizeText = QString("%1 KB").arg(m_fileInfo.getFileSize() / 1024);
    m_fileSizeLabel = new QLabel(sizeText, this);
    m_fileSizeLabel->setMinimumWidth(80);
    m_fileSizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      // 状态标签
    m_statusLabel = new QLabel(m_fileInfo.getStatusText(), this);
    m_statusLabel->setMinimumWidth(100);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    
    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setMinimumWidth(150);
    m_progressBar->setVisible(false);
    
    // 添加到布局
    m_layout->addWidget(m_fileNameLabel);
    m_layout->addWidget(m_fileSizeLabel);
    m_layout->addWidget(m_statusLabel);
    m_layout->addWidget(m_progressBar);
    m_layout->addStretch();
}

void FileItemWidget::updateProgress(int percentage) {
    m_progressBar->setValue(percentage);
    if (!m_progressBar->isVisible()) {
        m_progressBar->setVisible(true);
    }
}

void FileItemWidget::setStatus(const QString& status) {
    m_statusLabel->setText(status);
}

// FileListWidget 实现
FileListWidget::FileListWidget(QWidget* parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_listWidget(nullptr)
    , m_titleLabel(nullptr) {
    
    setupUI();
}

void FileListWidget::setupUI() {
    m_layout = new QVBoxLayout(this);
    
    // 标题标签
    m_titleLabel = new QLabel("文件列表", this);
    QFont font = m_titleLabel->font();
    font.setPointSize(12);
    font.setBold(true);
    m_titleLabel->setFont(font);
    m_layout->addWidget(m_titleLabel);
    
    // 文件列表widget
    m_listWidget = new QListWidget(this);
    m_listWidget->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_layout->addWidget(m_listWidget);
}

void FileListWidget::addFiles(const QStringList& filePaths) {
    QStringList addedFiles;
    
    for (const QString& filePath : filePaths) {
        QFileInfo fileInfo(filePath);
        
        if (!fileInfo.exists()) {
            continue;
        }
        
        // 检查是否已经存在
        bool exists = false;
        for (int i = 0; i < m_listWidget->count(); ++i) {
            QListWidgetItem* item = m_listWidget->item(i);
            FileItemWidget* widget = qobject_cast<FileItemWidget*>(m_listWidget->itemWidget(item));
            if (widget && widget->getFileInfo().getFilePath() == filePath) {
                exists = true;
                break;
            }
        }
        
        if (!exists) {
            Lusp_SyncUploadFileInfoHandler infoHandler(filePath.toStdU16String());
            FileInfoQtAdapter infoAdapter(infoHandler);
            
            // 创建列表项
            QListWidgetItem* listItem = new QListWidgetItem(m_listWidget);
            FileItemWidget* itemWidget = new FileItemWidget(infoAdapter, this);
            
            listItem->setSizeHint(itemWidget->sizeHint());
            m_listWidget->addItem(listItem);
            m_listWidget->setItemWidget(listItem, itemWidget);
            
            addedFiles << filePath;
        }
    }
    
    if (!addedFiles.isEmpty()) {
        emit filesAdded(addedFiles);
    }
    
    // 更新标题
    m_titleLabel->setText(QString("文件列表 (%1 个文件)").arg(m_listWidget->count()));
}

void FileListWidget::clearFiles() {
    m_listWidget->clear();
    m_titleLabel->setText("文件列表");
}

QStringList FileListWidget::getFilePaths() const {
    QStringList filePaths;
    
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        FileItemWidget* widget = qobject_cast<FileItemWidget*>(m_listWidget->itemWidget(item));
        if (widget) {
            filePaths << widget->getFileInfo().getFilePath();
        }
    }
    
    return filePaths;
}

int FileListWidget::getFileCount() const {
    return m_listWidget->count();
}
