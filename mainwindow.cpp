#include "mainwindow.h"
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QProgressBar>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QScrollBar>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_currentFolder()
    , m_selectedImageFile()
    , m_partitionAnalyzer(nullptr)
{
    setupUi();
    setupConnections();

    // Создаем анализатор разделов
    m_partitionAnalyzer = new PartitionAnalyzer(this);

    // Подключаем сигналы анализатора
    connect(m_partitionAnalyzer, &PartitionAnalyzer::logMessage,
            this, &MainWindow::displayLogMessage);
    connect(m_partitionAnalyzer, &PartitionAnalyzer::errorOccurred,
            this, &MainWindow::displayErrorMessage);

    displayLogMessage("Android Extractor initialized. Select a folder to begin.");
}

MainWindow::~MainWindow()
{
    // Qt автоматически удаляет дочерние объекты
}

void MainWindow::setupUi()
{
    // Основные настройки окна
    setWindowTitle("Android Extractor - Partition Analyzer");
    setMinimumSize(1200, 800);

    // Центральный виджет и основной layout
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // === Верхняя панель с кнопками ===
    QHBoxLayout *topLayout = new QHBoxLayout();

    m_btnOpenFolder = new QPushButton("Open Folder", this);
    m_btnOpenFolder->setFixedWidth(120);

    m_btnAnalyze = new QPushButton("Analyze File", this);
    m_btnAnalyze->setFixedWidth(120);
    m_btnAnalyze->setEnabled(false);

    m_lblFolderPath = new QLabel("No folder selected", this);
    m_lblFolderPath->setStyleSheet("QLabel { color: gray; }");

    m_lblSelectedFile = new QLabel("No file selected", this);
    m_lblSelectedFile->setStyleSheet("QLabel { color: gray; }");

    topLayout->addWidget(m_btnOpenFolder);
    topLayout->addWidget(m_btnAnalyze);
    topLayout->addWidget(m_lblFolderPath);
    topLayout->addStretch();
    topLayout->addWidget(m_lblSelectedFile);

    mainLayout->addLayout(topLayout);

    // === Основной сплиттер с содержимым ===
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Левая панель (файлы и разделы)
    QWidget *leftPanel = new QWidget(mainSplitter);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);

    // Метка для списка файлов
    QLabel *lblImageFiles = new QLabel("Image Files:", leftPanel);
    m_listImageFiles = new QListWidget(leftPanel);
    m_listImageFiles->setSelectionMode(QListWidget::SingleSelection);

    // Метка для списка разделов
    QLabel *lblPartitions = new QLabel("Partitions:", leftPanel);
    m_listPartitions = new QListWidget(leftPanel);
    m_listPartitions->setSelectionMode(QListWidget::SingleSelection);

    leftLayout->addWidget(lblImageFiles);
    leftLayout->addWidget(m_listImageFiles);
    leftLayout->addWidget(lblPartitions);
    leftLayout->addWidget(m_listPartitions);

    // Центральная панель (HEX просмотр и файловый менеджер)
    QSplitter *centerSplitter = new QSplitter(Qt::Vertical, mainSplitter);

    // HEX просмотрщик
    QWidget *hexPanel = new QWidget(centerSplitter);
    QVBoxLayout *hexLayout = new QVBoxLayout(hexPanel);
    QLabel *lblHex = new QLabel("Hex Viewer (first 4096 bytes):", hexPanel);
    m_hexViewer = new HexViewer(hexPanel);
    hexLayout->addWidget(lblHex);
    hexLayout->addWidget(m_hexViewer);

    // Файловый менеджер
    QWidget *filePanel = new QWidget(centerSplitter);
    QVBoxLayout *fileLayout = new QVBoxLayout(filePanel);
    QLabel *lblPartitionDetails = new QLabel("Partition Details:", filePanel);  // Исправлено имя переменной
    m_fileManager = new FileManager(filePanel);
    fileLayout->addWidget(lblPartitionDetails);  // Исправлено
    fileLayout->addWidget(m_fileManager);

    centerSplitter->addWidget(hexPanel);
    centerSplitter->addWidget(filePanel);
    centerSplitter->setSizes(QList<int>() << 400 << 300);

    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(centerSplitter);
    mainSplitter->setSizes(QList<int>() << 300 << 900);

    mainLayout->addWidget(mainSplitter);

    // === Прогресс бар ===
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setRange(0, 100);
    mainLayout->addWidget(m_progressBar);

    // === Лог вывод ===
    QLabel *lblLog = new QLabel("Log Output:", this);
    m_logOutput = new QTextEdit(this);
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumHeight(150);

    mainLayout->addWidget(lblLog);
    mainLayout->addWidget(m_logOutput);

    // Устанавливаем начальные состояния
    m_hexViewer->clearDisplay();
    m_fileManager->clearDisplay();
}

void MainWindow::setupConnections()
{
    // Подключаем кнопки
    connect(m_btnOpenFolder, &QPushButton::clicked,
            this, &MainWindow::openFolderDialog);
    connect(m_btnAnalyze, &QPushButton::clicked,
            this, &MainWindow::analyzeSelectedFile);

    // Подключаем списки
    connect(m_listImageFiles, &QListWidget::itemSelectionChanged,
            this, [this]() {
                QList<QListWidgetItem*> selected = m_listImageFiles->selectedItems();
                if (!selected.isEmpty()) {
                    m_selectedImageFile = selected.first()->text();
                    m_lblSelectedFile->setText("Selected: " + m_selectedImageFile);
                    m_btnAnalyze->setEnabled(true);

                    // Очищаем предыдущие результаты
                    m_listPartitions->clear();
                    m_hexViewer->clearDisplay();
                    m_fileManager->clearDisplay();
                } else {
                    m_btnAnalyze->setEnabled(false);
                }
            });

    connect(m_listPartitions, &QListWidget::currentRowChanged,
            this, &MainWindow::onPartitionSelected);
}

void MainWindow::openFolderDialog()
{
    // Открываем диалог выбора папки
    QString folder = QFileDialog::getExistingDirectory(
        this,
        "Select Folder with Image Files",
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

    if (!folder.isEmpty()) {
        m_currentFolder = folder;
        m_lblFolderPath->setText("Folder: " + folder);
        scanFolderForImageFiles(folder);

        displayLogMessage(QString("Opened folder: %1").arg(folder));

        // Очищаем предыдущие результаты
        m_listPartitions->clear();
        m_hexViewer->clearDisplay();
        m_fileManager->clearDisplay();
        m_selectedImageFile.clear();
        m_lblSelectedFile->setText("No file selected");
        m_btnAnalyze->setEnabled(false);
    }
}

void MainWindow::scanFolderForImageFiles(const QString &folderPath)
{
    QDir dir(folderPath);

    // Фильтр для файлов образов
    QStringList filters;
    filters << "*.img" << "*.bin" << "*.ext4" << "*.dsk" << "*.raw";

    // Получаем список файлов
    QStringList imageFiles = dir.entryList(filters, QDir::Files | QDir::Readable, QDir::Name);

    updateImageFileList(imageFiles);

    displayLogMessage(QString("Found %1 image files").arg(imageFiles.size()));
}

void MainWindow::updateImageFileList(const QStringList &fileList)
{
    m_listImageFiles->clear();

    if (fileList.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem("No image files found");
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(Qt::gray);
        m_listImageFiles->addItem(item);
    } else {
        m_listImageFiles->addItems(fileList);
    }
}

void MainWindow::analyzeSelectedFile()
{
    if (m_selectedImageFile.isEmpty() || m_currentFolder.isEmpty()) {
        displayErrorMessage("No file selected or folder not open");
        return;
    }

    // Формируем полный путь к файлу
    QString fullPath = m_currentFolder + "/" + m_selectedImageFile;

    displayLogMessage(QString("Starting analysis of: %1").arg(m_selectedImageFile));

    // Показываем прогресс бар
    m_progressBar->setVisible(true);
    m_progressBar->setValue(10);

    // Очищаем предыдущие результаты
    m_listPartitions->clear();
    m_hexViewer->clearDisplay();
    m_fileManager->clearDisplay();

    // Обновляем прогресс
    m_progressBar->setValue(30);

    // Анализируем диск
    m_currentDiskInfo = m_partitionAnalyzer->analyzeDisk(fullPath);

    // Обновляем прогресс
    m_progressBar->setValue(70);

    // Отображаем найденные разделы
    if (m_currentDiskInfo.partitions.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem("No partitions found");
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(Qt::gray);
        m_listPartitions->addItem(item);
    } else {
        for (const PartitionInfo &partition : m_currentDiskInfo.partitions) {
            QString displayText = QString("%1 (%2) - %3 bytes")
            .arg(partition.name)
                .arg(partition.description)
                .arg(partition.sizeBytes);
            m_listPartitions->addItem(displayText);
        }
    }

    // Завершаем прогресс
    m_progressBar->setValue(100);
    QTimer::singleShot(500, [this]() {
        m_progressBar->setVisible(false);
        m_progressBar->setValue(0);
    });

    displayLogMessage(QString("Analysis complete. Found %1 partitions")
                          .arg(m_currentDiskInfo.partitions.size()));
}

void MainWindow::onPartitionSelected(int index)
{
    if (index < 0 || index >= m_currentDiskInfo.partitions.size()) {
        m_hexViewer->clearDisplay();
        m_fileManager->clearDisplay();
        return;
    }

    const PartitionInfo &partition = m_currentDiskInfo.partitions[index];

    displayLogMessage(QString("Selected partition: %1").arg(partition.name));

    // Отображаем информацию о разделе в файловом менеджере
    QString partitionInfo = QString(
                                "Name: %1\n"
                                "Type: %2\n"
                                "Description: %3\n"
                                "Start: %4 bytes\n"
                                "Size: %5 bytes\n"
                                "Sectors: %6\n"
                                "File System: %7\n"
                                "Bootable: %8\n"
                                "GUID: %9"
                                ).arg(partition.name)
                                .arg(partition.type)
                                .arg(partition.description)
                                .arg(partition.startByte)
                                .arg(partition.sizeBytes)
                                .arg(partition.sizeSectors)
                                .arg(partition.fileSystem)
                                .arg(partition.bootable ? "Yes" : "No")
                                .arg(partition.guid.toString(QUuid::WithoutBraces));

    m_fileManager->displayPartitionInfo(partition.name, partitionInfo);

    // Читаем и отображаем первые 4096 байт раздела в HEX виде
    QByteArray data = m_partitionAnalyzer->readPartitionData(
        m_currentDiskInfo, index, 0, 4096
        );

    if (!data.isEmpty()) {
        m_hexViewer->setData(data);
    } else {
        m_hexViewer->clearDisplay();
        m_hexViewer->append("Failed to read partition data");
    }
}

void MainWindow::displayLogMessage(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString logMessage = QString("[%1] %2").arg(timestamp).arg(message);

    m_logOutput->append(logMessage);

    // Автоматическая прокрутка к новым сообщениям
    QScrollBar *scrollbar = m_logOutput->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void MainWindow::displayErrorMessage(const QString &errorMessage)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString logMessage = QString("[%1] ERROR: %2").arg(timestamp).arg(errorMessage);

    m_logOutput->append(logMessage);

    // Показываем сообщение об ошибке в диалоговом окне
    QMessageBox::critical(this, "Error", errorMessage);
}
