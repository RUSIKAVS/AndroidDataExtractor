#include "TesterWidget.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QFont>
#include <QFileInfo>
#include <QDateTime>
#include <QDialog>
#include <QTextEdit>
#include <QtConcurrent>
#include <iostream>
#include <iomanip>
#include <random>

class HexViewer : public QDialog {
public:
    HexViewer(const QByteArray& data, QWidget* parent = nullptr)
        : QDialog(parent) {
        setWindowTitle("Hex Viewer");
        resize(800, 600);

        QVBoxLayout* layout = new QVBoxLayout(this);

        QTextEdit* textEdit = new QTextEdit(this);
        textEdit->setReadOnly(true);
        textEdit->setFont(QFont("Courier New", 10));

        QString hexText;
        for (int i = 0; i < data.size(); i += 16) {
            hexText += QString("%1: ").arg(i, 8, 16, QChar('0')).toUpper();

            QString hexPart;
            QString asciiPart;
            for (int j = 0; j < 16; j++) {
                if (i + j < data.size()) {
                    unsigned char c = static_cast<unsigned char>(data[i + j]);
                    hexPart += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();

                    if (c >= 32 && c < 127) {
                        asciiPart += QChar(c);
                    } else {
                        asciiPart += ".";
                    }
                } else {
                    hexPart += "   ";
                    asciiPart += " ";
                }
            }

            hexText += hexPart + " " + asciiPart + "\n";
        }

        textEdit->setText(hexText);
        layout->addWidget(textEdit);

        QPushButton* closeBtn = new QPushButton("Close", this);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
        layout->addWidget(closeBtn);
    }
};

TesterWidget::TesterWidget(QWidget *parent)
    : QWidget(parent), extractor(this) {
    setupUI();
    connectSignals();
}

TesterWidget::~TesterWidget() {
}

void TesterWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* dirLayout = new QHBoxLayout();
    dirLayout->addWidget(new QLabel("Directory:"));

    directoryEdit = new QLineEdit();
    directoryEdit->setPlaceholderText("Select directory to scan...");
    directoryEdit->setMinimumWidth(300);
    dirLayout->addWidget(directoryEdit);

    browseBtn = new QPushButton("Browse...");
    dirLayout->addWidget(browseBtn);

    scanBtn = new QPushButton("Scan");
    dirLayout->addWidget(scanBtn);

    mainLayout->addLayout(dirLayout);

    progressBar = new QProgressBar();
    progressBar->setValue(0);
    progressBar->setTextVisible(true);
    mainLayout->addWidget(progressBar);

    tabWidget = new QTabWidget();

    QWidget* filesTab = new QWidget();
    QVBoxLayout* filesLayout = new QVBoxLayout(filesTab);
    filesTable = new QTableWidget();
    filesTable->setColumnCount(7);
    filesTable->setHorizontalHeaderLabels({
        "File Name", "Size", "Sector Size", "Created", "Type", "FS", "Is Dump"
    });
    filesTable->horizontalHeader()->setStretchLastSection(true);
    filesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    filesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    filesTable->setAlternatingRowColors(true);
    filesLayout->addWidget(filesTable);
    tabWidget->addTab(filesTab, "Files");

    QWidget* partitionsTab = new QWidget();
    QVBoxLayout* partitionsLayout = new QVBoxLayout(partitionsTab);
    partitionsTree = new QTreeWidget();
    partitionsTree->setColumnCount(6);
    QStringList headers;
    headers << "Name" << "Offset" << "Size" << "Sector Size" << "FS" << "Description";
    partitionsTree->setHeaderLabels(headers);
    partitionsTree->setAlternatingRowColors(true);
    partitionsLayout->addWidget(partitionsTree);
    tabWidget->addTab(partitionsTab, "Partitions");

    QWidget* fsTab = new QWidget();
    QVBoxLayout* fsLayout = new QVBoxLayout(fsTab);
    fsTable = new QTableWidget();
    fsTable->setColumnCount(7);
    fsTable->setHorizontalHeaderLabels({
        "Name", "Path", "Size", "Type", "Modified", "Permissions", "Owner"
    });
    fsTable->horizontalHeader()->setStretchLastSection(true);
    fsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    fsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    fsTable->setAlternatingRowColors(true);
    fsLayout->addWidget(fsTable);
    tabWidget->addTab(fsTab, "File System");

    mainLayout->addWidget(tabWidget);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    rawBtn = new QPushButton("Raw Access");
    buttonLayout->addWidget(rawBtn);

    hexBtn = new QPushButton("Hex View");
    buttonLayout->addWidget(hexBtn);

    structureBtn = new QPushButton("Structure");
    buttonLayout->addWidget(structureBtn);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
    setWindowTitle("Android Extractor Module Tester");
    resize(1200, 800);
}

void TesterWidget::connectSignals() {
    // Подключаем сигналы через лямбда-выражения
    connect(&extractor, &Extractor::scanProgress, this, &TesterWidget::onScanProgress);

    connect(browseBtn, &QPushButton::clicked, this, &TesterWidget::onBrowseClicked);
    connect(scanBtn, &QPushButton::clicked, this, &TesterWidget::onScanClicked);
    connect(rawBtn, &QPushButton::clicked, this, &TesterWidget::onRawAccessClicked);
    connect(hexBtn, &QPushButton::clicked, this, &TesterWidget::onHexViewClicked);
    connect(structureBtn, &QPushButton::clicked, this, &TesterWidget::onStructureClicked);

    connect(filesTable, &QTableWidget::itemClicked, this, &TesterWidget::onFileSelected);
    connect(partitionsTree, &QTreeWidget::itemClicked, this, &TesterWidget::onPartitionItemClicked);
}

void TesterWidget::onBrowseClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory",
                                                    QDir::homePath(),
                                                    QFileDialog::ShowDirsOnly);
    if (!dir.isEmpty()) {
        directoryEdit->setText(dir);
        currentDirectory = dir;
    }
}

void TesterWidget::onScanClicked() {
    QString dir = directoryEdit->text();
    if (dir.isEmpty() || !QDir(dir).exists()) {
        QMessageBox::warning(this, "Error", "Please select a valid directory");
        return;
    }

    progressBar->setValue(0);

    // Запускаем сканирование в отдельном потоке для UI-отзывчивости
    QtConcurrent::run([this, dir]() {
        currentFiles = extractor.scanDirectory(dir);

        // Обновляем UI в основном потоке
        QMetaObject::invokeMethod(this, [this]() {
            updateFileList(currentFiles);

            partitionsTree->clear();
            fsTable->setRowCount(0);

            if (currentFiles.isEmpty()) {
                QMessageBox::information(this, "Scan Complete", "No files found matching criteria");
            } else {
                QMessageBox::information(this, "Scan Complete",
                                         QString("Found %1 files").arg(currentFiles.size()));
            }
        });
    });
}

void TesterWidget::onScanProgress(int percent) {
    progressBar->setValue(percent);
}

void TesterWidget::updateFileList(const QList<AndroidExtractor::FileInfo>& files) {
    filesTable->setRowCount(files.size());

    for (int i = 0; i < files.size(); i++) {
        const auto& file = files[i];

        filesTable->setItem(i, 0, new QTableWidgetItem(file.fileName));
        filesTable->setItem(i, 1, new QTableWidgetItem(
                                      QString("%1 MB").arg(file.size / (1024.0 * 1024.0), 0, 'f', 2)));
        filesTable->setItem(i, 2, new QTableWidgetItem(
                                      QString("%1 bytes").arg(file.sectorSize)));
        filesTable->setItem(i, 3, new QTableWidgetItem(
                                      file.created.toString("dd.MM.yyyy hh:mm")));
        filesTable->setItem(i, 4, new QTableWidgetItem(file.partitionTableType));
        filesTable->setItem(i, 5, new QTableWidgetItem(file.fileSystem));
        filesTable->setItem(i, 6, new QTableWidgetItem(
                                      file.isDiskDump ? "Yes" : "No"));

        if (filesTable->item(i, 0)) {
            filesTable->item(i, 0)->setData(Qt::UserRole, file.fullPath);
        }
    }

    filesTable->resizeColumnsToContents();
}

void TesterWidget::onFileSelected() {
    QList<QTableWidgetItem*> selectedItems = filesTable->selectedItems();
    if (selectedItems.isEmpty()) return;

    int row = selectedItems.first()->row();
    QTableWidgetItem* item = filesTable->item(row, 0);
    if (!item) return;

    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty()) return;

    AndroidExtractor::FileInfo selectedFile;
    for (const auto& file : currentFiles) {
        if (file.fullPath == filePath) {
            selectedFile = file;
            break;
        }
    }

    if (selectedFile.isDiskDump) {
        // Анализируем в отдельном потоке
        QtConcurrent::run([this, filePath]() {
            QList<AndroidExtractor::FileInfo> partitions = extractor.analyzeDiskDump(filePath);

            QMetaObject::invokeMethod(this, [this, partitions]() {
                updatePartitionList(partitions);
                tabWidget->setCurrentIndex(1);
            });
        });
    }
}

void TesterWidget::onPartitionItemClicked(QTreeWidgetItem *item, int column) {
    if (!item || !item->parent()) return;

    QString filePath = item->data(0, Qt::UserRole).toString();
    qint64 offset = item->data(0, Qt::UserRole + 1).toLongLong();

    if (filePath.isEmpty()) return;

    // Анализируем в отдельном потоке
    QtConcurrent::run([this, filePath, offset]() {
        QList<AndroidExtractor::FileSystemEntry> entries = extractor.analyzePartition(filePath, offset);

        QMetaObject::invokeMethod(this, [this, entries]() {
            updateFileSystemView(entries);
            tabWidget->setCurrentIndex(2);
        });
    });
}

void TesterWidget::updatePartitionList(const QList<AndroidExtractor::FileInfo>& partitions) {
    partitionsTree->clear();

    if (partitions.isEmpty()) {
        QTreeWidgetItem* item = new QTreeWidgetItem(partitionsTree);
        item->setText(0, "No partitions found");
        return;
    }

    QTreeWidgetItem* root = new QTreeWidgetItem(partitionsTree);
    root->setText(0, "Disk Partitions");
    root->setExpanded(true);

    for (const auto& partition : partitions) {
        QTreeWidgetItem* item = new QTreeWidgetItem(root);
        item->setText(0, partition.partitionName.isEmpty() ?
                             QString("Partition %1").arg(partition.partitionNumber) :
                             partition.partitionName);
        item->setText(1, QString("0x%1").arg(partition.offset, 0, 16));
        item->setText(2, QString("%1 MB").arg(
                             partition.partitionSize / (1024.0 * 1024.0), 0, 'f', 2));
        item->setText(3, QString("%1 bytes").arg(partition.sectorSize));
        item->setText(4, partition.fileSystem);
        item->setText(5, partition.additionalProperties.value("Description", ""));

        item->setData(0, Qt::UserRole, partition.fullPath);
        item->setData(0, Qt::UserRole + 1, partition.offset);
    }

    partitionsTree->expandAll();
    partitionsTree->resizeColumnToContents(0);
}

void TesterWidget::updateFileSystemView(const QList<AndroidExtractor::FileSystemEntry>& entries) {
    fsTable->setRowCount(entries.size());

    for (int i = 0; i < entries.size(); i++) {
        const auto& entry = entries[i];

        fsTable->setItem(i, 0, new QTableWidgetItem(entry.name));
        fsTable->setItem(i, 1, new QTableWidgetItem(entry.path));
        fsTable->setItem(i, 2, new QTableWidgetItem(
                                   QString("%1").arg(entry.size)));
        fsTable->setItem(i, 3, new QTableWidgetItem(
                                   entry.isDirectory ? "Directory" : "File"));
        fsTable->setItem(i, 4, new QTableWidgetItem(
                                   entry.modified.toString("dd.MM.yyyy hh:mm:ss")));
        fsTable->setItem(i, 5, new QTableWidgetItem(entry.permissions));
        fsTable->setItem(i, 6, new QTableWidgetItem(
                                   QString("%1:%2").arg(entry.owner).arg(entry.group)));
    }

    fsTable->resizeColumnsToContents();
}

void TesterWidget::onRawAccessClicked() {
    QList<QTableWidgetItem*> selected = filesTable->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select a file first");
        return;
    }

    int row = selected.first()->row();
    QTableWidgetItem* item = filesTable->item(row, 0);
    if (!item) {
        QMessageBox::warning(this, "Error", "No file selected");
        return;
    }

    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "Error", "No file selected");
        return;
    }

    showRawData(filePath);
}

void TesterWidget::onHexViewClicked() {
    QList<QTableWidgetItem*> selected = filesTable->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select a file first");
        return;
    }

    int row = selected.first()->row();
    QTableWidgetItem* item = filesTable->item(row, 0);
    if (!item) {
        QMessageBox::warning(this, "Error", "No file selected");
        return;
    }

    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "Error", "No file selected");
        return;
    }

    showHexView(filePath);
}

void TesterWidget::onStructureClicked() {
    QList<QTableWidgetItem*> selected = filesTable->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select a file first");
        return;
    }

    int row = selected.first()->row();
    QTableWidgetItem* item = filesTable->item(row, 0);
    if (!item) {
        QMessageBox::warning(this, "Error", "No file selected");
        return;
    }

    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "Error", "No file selected");
        return;
    }

    showStructureAnalysis(filePath);
}

void TesterWidget::showRawData(const QString& filePath, qint64 offset) {
    auto rawAccess = extractor.getRawAccess(filePath, offset);
    if (!rawAccess) {
        QMessageBox::warning(this, "Error", "Cannot get raw access to file");
        return;
    }

    QByteArray data = rawAccess->readData(0, 1024);

    if (data.isEmpty()) {
        QMessageBox::warning(this, "Error", "Cannot read data from file");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Raw Data Viewer - " + QFileInfo(filePath).fileName());
    dialog.resize(600, 400);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QTextEdit* textEdit = new QTextEdit(&dialog);
    textEdit->setReadOnly(true);
    textEdit->setFont(QFont("Courier New", 10));

    QString displayText = QString("File: %1\nSize: %2 bytes\n\n")
                              .arg(QFileInfo(filePath).fileName())
                              .arg(data.size());

    for (int i = 0; i < data.size(); i += 16) {
        QString hexPart;
        QString asciiPart;

        for (int j = 0; j < 16; j++) {
            if (i + j < data.size()) {
                unsigned char c = static_cast<unsigned char>(data[i + j]);
                hexPart += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();

                if (c >= 32 && c < 127) {
                    asciiPart += QChar(c);
                } else {
                    asciiPart += ".";
                }
            } else {
                hexPart += "   ";
                asciiPart += " ";
            }
        }

        displayText += QString("%1: %2 |%3|\n")
                           .arg(i, 8, 16, QChar('0')).toUpper()
                           .arg(hexPart)
                           .arg(asciiPart);
    }

    textEdit->setText(displayText);
    layout->addWidget(textEdit);

    QPushButton* closeBtn = new QPushButton("Close", &dialog);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    dialog.exec();
}

void TesterWidget::showHexView(const QString& filePath, qint64 offset) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Cannot open file: " + file.errorString());
        return;
    }

    QByteArray data = file.read(4096);
    file.close();

    if (data.isEmpty()) {
        QMessageBox::warning(this, "Error", "Cannot read data from file");
        return;
    }

    HexViewer* viewer = new HexViewer(data, this);
    viewer->exec();
    delete viewer;
}

void TesterWidget::showStructureAnalysis(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Cannot open file: " + file.errorString());
        return;
    }

    QByteArray data = file.read(8192);
    file.close();

    QDialog dialog(this);
    dialog.setWindowTitle("Structure Analysis - " + QFileInfo(filePath).fileName());
    dialog.resize(800, 600);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QTextEdit* textEdit = new QTextEdit(&dialog);
    textEdit->setReadOnly(true);
    textEdit->setFont(QFont("Courier New", 10));

    QString analysisText = "Structure Analysis\n";
    analysisText += "==================\n\n";

    AndroidExtractor::FileInfo fileInfo = extractor.getFileInfo(filePath);

    analysisText += QString("File name: %1\n").arg(fileInfo.fileName);
    analysisText += QString("File size: %1 MB\n").arg(fileInfo.size / (1024.0 * 1024.0), 0, 'f', 2);
    analysisText += QString("Sector size: %1 bytes\n").arg(fileInfo.sectorSize);
    analysisText += QString("Total sectors: %1\n").arg(fileInfo.sectorCount);
    analysisText += QString("Detected type: %1\n").arg(fileInfo.partitionTableType);
    analysisText += QString("File system: %1\n").arg(fileInfo.fileSystem);
    analysisText += QString("Is disk dump: %1\n").arg(fileInfo.isDiskDump ? "Yes" : "No");
    analysisText += QString("Has GPT: %1\n").arg(fileInfo.hasGPT ? "Yes" : "No");
    analysisText += QString("Has MBR: %1\n").arg(fileInfo.hasMBR ? "Yes" : "No");

    // Добавляем debug информацию
    if (!fileInfo.debugInfo.isEmpty()) {
        analysisText += "\nDebug Information:\n";
        analysisText += fileInfo.debugInfo;
    }

    // Добавляем информацию о разделах
    if (fileInfo.isDiskDump) {
        QList<AndroidExtractor::FileInfo> partitions = extractor.analyzeDiskDump(filePath);
        analysisText += QString("\n\nFound %1 partitions:\n").arg(partitions.size());

        for (const auto& partition : partitions) {
            analysisText += QString("\nPartition %1: %2\n").arg(partition.partitionNumber).arg(partition.partitionName);
            analysisText += QString("  Offset: 0x%1\n").arg(partition.offset, 0, 16);
            analysisText += QString("  Size: %1 MB\n").arg(partition.partitionSize / (1024.0 * 1024.0), 0, 'f', 2);
            analysisText += QString("  Sector Size: %1 bytes\n").arg(partition.sectorSize);
            analysisText += QString("  File System: %1\n").arg(partition.fileSystem);
        }

        if (partitions.isEmpty()) {
            analysisText += "\nNo partitions found!\n";
            analysisText += "Possible reasons:\n";
            analysisText += "1. GPT table is corrupted\n";
            analysisText += "2. Non-standard GPT layout\n";
            analysisText += "3. File is not a complete disk dump\n";
        }
    }

    analysisText += "\nHex dump of first " + QString::number(data.size()) + " bytes:\n";

    for (int i = 0; i < data.size(); i += 16) {
        analysisText += QString("%1: ").arg(i, 8, 16, QChar('0')).toUpper();

        QString hexPart;
        QString asciiPart;
        for (int j = 0; j < 16; j++) {
            if (i + j < data.size()) {
                unsigned char c = static_cast<unsigned char>(data[i + j]);
                hexPart += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();

                if (c >= 32 && c < 127) {
                    asciiPart += QChar(c);
                } else {
                    asciiPart += ".";
                }
            } else {
                hexPart += "   ";
                asciiPart += " ";
            }
        }

        analysisText += hexPart + " " + asciiPart + "\n";
    }

    textEdit->setText(analysisText);
    layout->addWidget(textEdit);

    QPushButton* closeBtn = new QPushButton("Close", &dialog);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    dialog.exec();
}
