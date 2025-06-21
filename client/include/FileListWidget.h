#pragma once

#include <QWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QString>
#include "FileInfoQtAdapter.h"

class FileItemWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileItemWidget(const FileInfoQtAdapter& fileInfo, QWidget* parent = nullptr);
    
    const FileInfoQtAdapter& getFileInfo() const { return m_fileInfo; }
    void updateProgress(int percentage);
    void setStatus(const QString& status);

private:
    void setupUI();

private:
    FileInfoQtAdapter m_fileInfo;
    QHBoxLayout* m_layout;
    QLabel* m_fileNameLabel;
    QLabel* m_fileSizeLabel;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
};

class FileListWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileListWidget(QWidget* parent = nullptr);
    
    void addFiles(const QStringList& filePaths);
    void clearFiles();
    QStringList getFilePaths() const;
    int getFileCount() const;

signals:
    void filesAdded(const QStringList& filePaths);

private:
    void setupUI();

private:
    QVBoxLayout* m_layout;
    QListWidget* m_listWidget;
    QLabel* m_titleLabel;
};
