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
    void setupUploadQueue();
    void addFilesToList(const QStringList& filePaths);
    void addFilesToUploadQueue(const QStringList& filePaths);

private:

    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;    QHBoxLayout* m_buttonLayout;
    
    FileListWidget* m_fileListWidget;
    QPushButton* m_selectFilesBtn;
    QPushButton* m_uploadBtn;
    QPushButton* m_clearListBtn;
    
    QLabel* m_statusLabel;
    QProgressBar* m_overallProgressBar;
    
};
