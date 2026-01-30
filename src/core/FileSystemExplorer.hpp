#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QTextEdit>
#include <QFileSystemModel>
#include <QTreeView>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include "AndroidPartition.hpp"

class FileSystemExplorer : public QWidget
{
    Q_OBJECT

public:
    explicit FileSystemExplorer(QWidget *parent = nullptr);
    ~FileSystemExplorer();

    bool mountPartition(const AndroidPartition& partition,
                        const QByteArray& decryptionKey = QByteArray());
    bool unmountPartition();

    QString getMountedPath() const;
    bool isMounted() const;

    void extractSelectedFiles(const QString& outputDir);
    void extractAllFiles(const QString& outputDir);

    QStringList getSelectedFiles() const;
    QStringList getAllFiles() const;

    static bool supportsFilesystem(const QString& fsType);

signals:
    void fileSelected(const QString& filePath);
    void extractionProgress(int percent);
    void extractionFinished(bool success);

private slots:
    void onFileClicked(const QModelIndex& index);
    void onContextMenuRequested(const QPoint& pos);
    void refreshView();
    void showFileInfo(const QString& filePath);

private:
    void setupUI();
    void setupContextMenu();

    bool mountExt4(const QString& partitionPath, const QString& mountPoint);
    bool mountF2fs(const QString& partitionPath, const QString& mountPoint);
    bool mountErofs(const QString& partitionPath, const QString& mountPoint);
    bool mountFat(const QString& partitionPath, const QString& mountPoint);

    QString createMountPoint();
    bool executeMountCommand(const QString& cmd, const QStringList& args);

    QFileSystemModel* fileModel;
    QTreeView* treeView;
    QTextEdit* fileInfoText;
    QLabel* statusLabel;

    QString currentMountPoint;
    QString partitionPath;
    bool mounted;

    QStringList supportedFilesystems = {
        "ext4", "ext3", "ext2", "f2fs", "erofs",
        "fat", "fat32", "vfat", "exfat", "ntfs"
    };
};
