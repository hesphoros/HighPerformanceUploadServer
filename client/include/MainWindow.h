#pragma once

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QStringList>

class FileListWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onSelectFilesClicked();
    void onUploadClicked();
    void onClearListClicked();

private:
    void setupUI();
    void connectSignals();
    void setupUploadQueue();  // 设置极简上传队列
    void addFilesToList(const QStringList& filePaths);
    void addFilesToUploadQueue(const QStringList& filePaths);  // 🎯 极简上传接口

private:
    // UI组件
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;    QHBoxLayout* m_buttonLayout;
    
    FileListWidget* m_fileListWidget;
    QPushButton* m_selectFilesBtn;
    QPushButton* m_uploadBtn;
    QPushButton* m_clearListBtn;
    
    QLabel* m_statusLabel;
    QProgressBar* m_overallProgressBar;
    
    // 注意：不再需要UploadManager！UI极简，只需要上传队列
};
