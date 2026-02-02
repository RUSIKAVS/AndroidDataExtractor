#ifndef TESTERWIDGET_H
#define TESTERWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QTextEdit>
#include "AndroidExtractor.h"

class TesterWidget : public QWidget {
    Q_OBJECT

public:
    explicit TesterWidget(QWidget *parent = nullptr);
    ~TesterWidget();

private slots:
    void onBrowseClicked();
    void onScanClicked();
    void onRawAccessClicked();
    void onHexViewClicked();
    void onStructureClicked();
    void onFileSelected();
    void onPartitionItemClicked(QTreeWidgetItem *item, int column);
    void onScanProgress(int percent);

private:
    void setupUI();
    void connectSignals();
    void updateFileList(const QList<AndroidExtractor::FileInfo>& files);
    void updatePartitionList(const QList<AndroidExtractor::FileInfo>& partitions);
    void updateFileSystemView(const QList<AndroidExtractor::FileSystemEntry>& entries);
    void showRawData(const QString& filePath, qint64 offset = 0);
    void showHexView(const QString& filePath, qint64 offset = 0);
    void showStructureAnalysis(const QString& filePath);

    Extractor extractor;  // Используем Extractor вместо AndroidExtractor::Extractor
    QString currentDirectory;
    QList<AndroidExtractor::FileInfo> currentFiles;

    // UI элементы
    QLineEdit* directoryEdit;
    QPushButton* browseBtn;
    QPushButton* scanBtn;
    QProgressBar* progressBar;
    QTabWidget* tabWidget;
    QTableWidget* filesTable;
    QTreeWidget* partitionsTree;
    QTableWidget* fsTable;
    QPushButton* rawBtn;
    QPushButton* hexBtn;
    QPushButton* structureBtn;
};

#endif // TESTERWIDGET_H
