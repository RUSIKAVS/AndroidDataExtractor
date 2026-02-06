#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "partitionanalyzer.h"
#include "hexviewer.h"
#include "guidmanager.h"
#include "filemanager.h"
#include "superanalyzer.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QHeaderView>
#include <QTreeWidgetItem>
#include <QInputDialog>
#include <QClipboard>
#include <QStandardPaths>
#include <QSettings>
#include <QDateTime>
#include <QTextStream>
#include <QThread>
#include <QDebug>
#include <QTextCursor>
#include <QFont>
#include <QFontMetrics>
#include <QTableWidget>  // Добавьте этот заголовок

/**
 * @brief Конструктор главного окна
 * @param parent Родительский виджет
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_partitionAnalyzer(nullptr)
    , m_hexViewer(nullptr)
    , m_guidManager(nullptr)
    , m_fileManager(nullptr)
    , m_superAnalyzer(nullptr)
    , m_contextMenu(nullptr)
    , m_selectedPartitionRow(-1)
{
    ui->setupUi(this);

    // Инициализация компонентов
    m_partitionAnalyzer = new PartitionAnalyzer(this);
    m_hexViewer = new HexViewer(this);
    m_guidManager = new GuidManager(this);
    m_fileManager = new FileManager(this);
    m_superAnalyzer = new SuperAnalyzer(this);
    m_contextMenu = new QMenu(this);

    // Включаем подробное логирование
    m_partitionAnalyzer->setVerboseLogging(true);

    // Настройка интерфейса
    setupUi();

    // Настройка таблицы разделов
    setupPartitionsTable();

    // Настройка дерева файлов
    setupFileTree();

    // Настройка соединений сигналов и слотов
    setupConnections();

    // Загрузка настроек
    loadSettings();

    // Начальное сообщение в лог
    logMessage("Android Data Extractor инициализирован");
    logMessage("Выберите файл для анализа");

    qDebug() << "MainWindow: Инициализация завершена";
}

/**
 * @brief Логирует сообщение
 * @param message Сообщение для логирования
 */
void MainWindow::logMessage(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("[HH:mm:ss]");
    QString fullMessage = timestamp + " " + message;

    // Отправляем в onLogMessage для отображения в UI
    onLogMessage(fullMessage);
    qDebug().noquote() << fullMessage;
}

/**
 * @brief Настройка таблицы разделов
 */
void MainWindow::setupPartitionsTable()
{
    // Проверяем, существует ли таблица
    if (!ui->partitionsTable) {
        qDebug() << "Внимание: таблица partitionsTable не найдена в UI!";
        return;
    }

    // Установка заголовков таблицы разделов
    QStringList headers = {"Имя", "Смещение (hex)", "Размер", "Тип", "GUID", "Статус", "LBA"};
    ui->partitionsTable->setColumnCount(headers.size());
    ui->partitionsTable->setHorizontalHeaderLabels(headers);

    // Настройка поведения таблицы
    ui->partitionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->partitionsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->partitionsTable->setAlternatingRowColors(true);
    ui->partitionsTable->setSortingEnabled(true);
    ui->partitionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->partitionsTable->setContextMenuPolicy(Qt::CustomContextMenu);

    // Настройка заголовков
    QHeaderView* header = ui->partitionsTable->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Interactive);   // Имя
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents); // Смещение
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents); // Размер
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Тип
    header->setSectionResizeMode(4, QHeaderView::Interactive);   // GUID
    header->setSectionResizeMode(5, QHeaderView::ResizeToContents); // Статус
    header->setSectionResizeMode(6, QHeaderView::ResizeToContents); // LBA

    // Установка ширины колонок
    ui->partitionsTable->setColumnWidth(0, 120);  // Имя
    ui->partitionsTable->setColumnWidth(1, 100);  // Смещение
    ui->partitionsTable->setColumnWidth(2, 100);  // Размер
    ui->partitionsTable->setColumnWidth(3, 120);  // Тип
    ui->partitionsTable->setColumnWidth(4, 250);  // GUID
    ui->partitionsTable->setColumnWidth(5, 100);  // Статус
    ui->partitionsTable->setColumnWidth(6, 80);   // LBA

    logMessage("Таблица разделов настроена");
}


/**
 * @brief Деструктор главного окна
 */
MainWindow::~MainWindow()
{
    // Сохранение настроек перед закрытием
    saveSettings();

    // Объекты с родителем this будут удалены автоматически Qt
    delete ui;

    qDebug() << "MainWindow: Деструктор вызван";
}

/**
 * @brief Настройка дерева файлов
 */
void MainWindow::setupFileTree()
{
    // Установка заголовков для дерева файлов
    ui->fileTreeWidget->setColumnCount(3);
    ui->fileTreeWidget->setHeaderLabels({"Имя файла", "Размер", "Тип"});

    // Настройка дерева файлов
    ui->fileTreeWidget->setAlternatingRowColors(true);
    ui->fileTreeWidget->setAnimated(true);
    ui->fileTreeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->fileTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    // Настройка заголовков дерева
    QHeaderView* treeHeader = ui->fileTreeWidget->header();
    treeHeader->setStretchLastSection(false);
    treeHeader->setSectionResizeMode(0, QHeaderView::Stretch);  // Имя файла
    treeHeader->setSectionResizeMode(1, QHeaderView::ResizeToContents);  // Размер
    treeHeader->setSectionResizeMode(2, QHeaderView::ResizeToContents);  // Тип

    logMessage("Дерево файлов настроено");
}

/**
 * @brief Настройка пользовательского интерфейса
 */
void MainWindow::setupUi()
{
    // Настройка заголовка окна
    setWindowTitle("Android Data Extractor v1.0");

    // Настройка прогресс-бара
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    ui->progressBar->setTextVisible(true);
    ui->progressBar->setFormat("%p%");

    // Настройка Hex Viewer
    if (m_hexViewer) {
        // HexViewer теперь сам является QWidget
        ui->scrollArea->setWidget(m_hexViewer);
        logMessage("Hex Viewer установлен в scrollArea");

        // Загружаем тестовые данные для демонстрации
        QByteArray testData;
        for (int i = 0; i < 256; ++i) {
            testData.append(static_cast<char>(i));
        }
        m_hexViewer->loadDataFromMemory(testData);
        logMessage("Загружены тестовые данные в Hex Viewer (256 байт)");
    } else {
        logMessage("ВНИМАНИЕ: Hex Viewer не инициализирован");
    }

    // Настройка текстового поля для суперблока
    ui->superBlockText->setFont(QFont("Courier New", 10));
    ui->superBlockText->setReadOnly(true);
    ui->superBlockText->setWordWrapMode(QTextOption::NoWrap);

    // Установка начального пути для извлечения
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                          + "/AndroidExtractor";
    ui->extractPathEdit->setText(defaultPath);

    // Настройка контекстного меню для таблицы разделов
    if (!m_contextMenu) {
        m_contextMenu = new QMenu(this);
    }

    m_contextMenu->addAction("Копировать имя раздела", this, &MainWindow::copyPartitionName);
    m_contextMenu->addAction("Копировать GUID", this, &MainWindow::copyPartitionGuid);
    m_contextMenu->addAction("Копировать смещение", this, &MainWindow::copyPartitionOffset);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction("Показать в Hex Viewer", this, &MainWindow::showInHexViewer);
    m_contextMenu->addAction("Экспортировать информацию", this, [this]() {
        QMessageBox::information(this, "Информация", "Функция экспорта в разработке");
    });

    // Настройка шрифта для логов
    QFont logFont("Monospace", 9);
    logFont.setStyleHint(QFont::TypeWriter);
    ui->logTextEdit->setFont(logFont);

    logMessage("Пользовательский интерфейс настроен");
}

/**
 * @brief Настройка соединений сигналов и слотов
 */
void MainWindow::setupConnections()
{
    // ========== Кнопки управления файлами ==========
    connect(ui->browseButton, &QPushButton::clicked,
            this, &MainWindow::onBrowseClicked);
    connect(ui->scanButton, &QPushButton::clicked,
            this, &MainWindow::onScanClicked);

    // ========== Кнопки извлечения данных ==========
    connect(ui->extractButton, &QPushButton::clicked,
            this, &MainWindow::onExtractClicked);
    connect(ui->extractAllButton, &QPushButton::clicked,
            this, &MainWindow::onExtractAllClicked);

    // ========== Кнопки файлового менеджера ==========
    connect(ui->refreshFilesButton, &QPushButton::clicked,
            this, &MainWindow::onRefreshFilesClicked);
    connect(ui->extractFileButton, &QPushButton::clicked,
            this, &MainWindow::onExtractFileClicked);
    connect(ui->findFilesButton, &QPushButton::clicked,
            this, &MainWindow::onFindFilesClicked);

    // ========== Кнопки анализа суперблока ==========
    connect(ui->analyzeSuperButton, &QPushButton::clicked,
            this, &MainWindow::onAnalyzeSuperClicked);

    // ========== Кнопки настроек ==========
    connect(ui->browseExtractButton, &QPushButton::clicked,
            this, &MainWindow::onBrowseExtractClicked);
    connect(ui->saveSettingsButton, &QPushButton::clicked,
            this, &MainWindow::onSaveSettingsClicked);
    connect(ui->defaultSettingsButton, &QPushButton::clicked,
            this, &MainWindow::onDefaultSettingsClicked);

    // ========== Кнопка сохранения отчета ==========
    connect(ui->saveReportButton, &QPushButton::clicked,
            this, &MainWindow::onSaveReportClicked);

    // ========== Контекстное меню таблицы разделов ==========
    connect(ui->partitionsTable, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::showPartitionContextMenu);

    // ========== Двойные клики ==========
    connect(ui->partitionsTable, &QTableWidget::itemDoubleClicked,
            this, &MainWindow::onPartitionDoubleClicked);
    connect(ui->fileTreeWidget, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onFileDoubleClicked);

    // ========== Сигналы от PartitionAnalyzer ==========
    if (m_partitionAnalyzer) {
        connect(m_partitionAnalyzer, &PartitionAnalyzer::progressUpdated,
                ui->progressBar, &QProgressBar::setValue);
        connect(m_partitionAnalyzer, &PartitionAnalyzer::analysisComplete,
                this, &MainWindow::onAnalysisComplete);
        connect(m_partitionAnalyzer, &PartitionAnalyzer::errorOccurred,
                this, &MainWindow::showError);
        connect(m_partitionAnalyzer, &PartitionAnalyzer::logMessage,
                this, &MainWindow::onLogMessage);
    }

    // ========== Кнопки управления логами ==========
    connect(ui->clearLogButton, &QPushButton::clicked,
            this, &MainWindow::onClearLogClicked);
    connect(ui->saveLogButton, &QPushButton::clicked,
            this, &MainWindow::onSaveLogClicked);
    connect(ui->copyLogButton, &QPushButton::clicked,
            this, &MainWindow::onCopyLogClicked);
    connect(ui->autoScrollCheckBox, &QCheckBox::stateChanged,
            this, &MainWindow::onAutoScrollChanged);

    // ========== Меню Файл ==========
    connect(ui->actionOpen, &QAction::triggered,
            this, &MainWindow::onBrowseClicked);
    connect(ui->actionExit, &QAction::triggered,
            this, &QWidget::close);

    // ========== Меню Инструменты ==========
    connect(ui->actionAnalyzePartitions, &QAction::triggered,
            this, &MainWindow::onScanClicked);
    connect(ui->actionHexView, &QAction::triggered, this, [this]() {
        ui->tabWidget->setCurrentIndex(1);  // Переключаем на вкладку Hex Viewer
        logMessage("Переключено на вкладку Hex Viewer");
    });
    connect(ui->actionScanFiles, &QAction::triggered,
            this, &MainWindow::onRefreshFilesClicked);

    // ========== Меню Справка ==========
    connect(ui->actionViewLogs, &QAction::triggered, this, [this]() {
        ui->tabWidget->setCurrentIndex(5); // Переключаем на вкладку логов
        logMessage("Переключено на вкладку логов");
    });

    logMessage("Соединения сигналов и слотов настроены");
}

/**
 * @brief Загрузка настроек приложения
 */
void MainWindow::loadSettings()
{
    QSettings settings("AndroidExtractor", "AndroidDataExtractor");

    // Загрузка пути для извлечения
    QString extractPath = settings.value("extractPath",
                                         QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/AndroidExtractor").toString();
    ui->extractPathEdit->setText(extractPath);

    // Загрузка настроек логов
    bool saveLogs = settings.value("saveLogs", true).toBool();
    ui->saveLogsCheckBox->setChecked(saveLogs);

    // Загрузка уровня детализации
    int detailLevel = settings.value("detailLevel", 1).toInt();
    ui->detailLevelCombo->setCurrentIndex(qBound(0, detailLevel, 2));

    // Загрузка настройки автопрокрутки логов
    bool autoScroll = settings.value("autoScroll", true).toBool();
    ui->autoScrollCheckBox->setChecked(autoScroll);

    // Загрузка геометрии окна
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }

    // Загрузка состояния окна
    if (settings.contains("windowState")) {
        restoreState(settings.value("windowState").toByteArray());
    }

    // Загрузка последнего пути к файлу
    QString lastFilePath = settings.value("lastFilePath", "").toString();
    if (!lastFilePath.isEmpty() && QFileInfo::exists(lastFilePath)) {
        ui->filePathEdit->setText(lastFilePath);
    }

    // Загрузка индекса текущей вкладки
    int currentTab = settings.value("currentTab", 0).toInt();
    ui->tabWidget->setCurrentIndex(qBound(0, currentTab, ui->tabWidget->count() - 1));

    logMessage("Настройки загружены");
}

/**
 * @brief Сохранение настроек приложения
 */
void MainWindow::saveSettings()
{
    QSettings settings("AndroidExtractor", "AndroidDataExtractor");

    // Сохранение пути для извлечения
    settings.setValue("extractPath", ui->extractPathEdit->text());

    // Сохранение настроек логов
    settings.setValue("saveLogs", ui->saveLogsCheckBox->isChecked());

    // Сохранение уровня детализации
    settings.setValue("detailLevel", ui->detailLevelCombo->currentIndex());

    // Сохранение настройки автопрокрутки логов
    settings.setValue("autoScroll", ui->autoScrollCheckBox->isChecked());

    // Сохранение геометрии окна
    settings.setValue("geometry", saveGeometry());

    // Сохранение состояния окна
    settings.setValue("windowState", saveState());

    // Сохранение последнего пути к файлу
    if (!ui->filePathEdit->text().isEmpty()) {
        settings.setValue("lastFilePath", ui->filePathEdit->text());
    }

    // Сохранение индекса текущей вкладки
    settings.setValue("currentTab", ui->tabWidget->currentIndex());

    settings.sync();
    logMessage("Настройки сохранены");
}

/**
 * @brief Слот для обработки нажатия кнопки "Обзор..."
 */
void MainWindow::onBrowseClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Выберите файл образа или устройство",
                                                    QDir::homePath(),
                                                    "Все файлы (*.*);;"
                                                    "Образы дисков (*.img *.bin *.raw *.dmp);;"
                                                    "Файлы Android (*.img *.mbn *.sin);;"
                                                    "Текстовые файлы (*.txt *.log)");

    if (!fileName.isEmpty()) {
        ui->filePathEdit->setText(fileName);
        logMessage(QString("Выбран файл: %1").arg(fileName));

        // Проверяем размер файла
        QFileInfo fileInfo(fileName);
        if (fileInfo.exists()) {
            qint64 fileSize = fileInfo.size();
            logMessage(QString("Размер файла: %1 байт (%2)").arg(fileSize).arg(formatFileSize(fileSize)));
        }
    }
}

/**
 * @brief Слот для обработки нажатия кнопки "Сканировать"
 */
void MainWindow::onScanClicked()
{
    QString filePath = ui->filePathEdit->text().trimmed();

    if (filePath.isEmpty()) {
        showError("Пожалуйста, выберите файл для анализа.");
        return;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        showError(QString("Файл '%1' не существует.").arg(filePath));
        return;
    }

    if (!fileInfo.isReadable()) {
        showError(QString("Файл '%1' недоступен для чтения.").arg(filePath));
        return;
    }

    logMessage(QString("Начинаю анализ файла: %1").arg(filePath));

    // Очистка предыдущих результатов
    ui->partitionsTable->setRowCount(0);
    ui->fileTreeWidget->clear();
    ui->superBlockText->clear();

    // Сброс прогресс-бара
    ui->progressBar->setValue(0);

    // Блокировка интерфейса на время анализа
    setUiEnabled(false);

    // Запуск анализа
    bool success = m_partitionAnalyzer->analyzePartitions(filePath);

    if (!success) {
        QString errorMsg = m_partitionAnalyzer->lastError();
        if (errorMsg.isEmpty()) {
            errorMsg = "Анализ разделов завершился с ошибкой.";
        }
        showError(errorMsg);
        setUiEnabled(true);
    }

    // Разблокировка интерфейса произойдет в onAnalysisComplete или showError
}

/**
 * @brief Слот для обработки завершения анализа разделов
 * @param success Успешность анализа
 */
void MainWindow::onAnalysisComplete(bool success)
{
    setUiEnabled(true);

    if (!success) {
        showError("Анализ разделов завершился с ошибкой.");
        return;
    }

    // Получение результатов анализа
    const auto& partitions = m_partitionAnalyzer->getPartitions();

    logMessage(QString("Анализ завершен. Найдено %1 разделов").arg(partitions.size()));

    // Отображение разделов в таблице
    ui->partitionsTable->setRowCount(partitions.size());

    for (int i = 0; i < partitions.size(); ++i) {
        const auto& part = partitions[i];

        // Создаем элементы таблицы
        QTableWidgetItem* nameItem = new QTableWidgetItem(part.name);
        QTableWidgetItem* offsetItem = new QTableWidgetItem(QString("0x%1").arg(part.offset, 0, 16));
        QTableWidgetItem* sizeItem = new QTableWidgetItem(formatFileSize(part.size));
        QTableWidgetItem* typeItem = new QTableWidgetItem(part.type);
        QTableWidgetItem* guidItem = new QTableWidgetItem(part.guid);
        QTableWidgetItem* statusItem = new QTableWidgetItem(part.isMounted ? "Смонтирован" : "Не смонтирован");
        QTableWidgetItem* lbaItem = new QTableWidgetItem(QString("%1-%2").arg(part.startLba).arg(part.endLba));

        // Устанавливаем выравнивание
        offsetItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lbaItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // Устанавливаем всплывающие подсказки
        nameItem->setToolTip(part.name);
        offsetItem->setToolTip(QString("Смещение: %1 байт").arg(part.offset));
        sizeItem->setToolTip(QString("Размер: %1 байт").arg(part.size));
        typeItem->setToolTip(QString("Тип: %1\nGUID типа: %2").arg(part.type).arg(part.typeGuid));
        guidItem->setToolTip(part.guid);
        lbaItem->setToolTip(QString("LBA: %1 - %2").arg(part.startLba).arg(part.endLba));

        // Добавляем элементы в таблицу
        ui->partitionsTable->setItem(i, 0, nameItem);
        ui->partitionsTable->setItem(i, 1, offsetItem);
        ui->partitionsTable->setItem(i, 2, sizeItem);
        ui->partitionsTable->setItem(i, 3, typeItem);
        ui->partitionsTable->setItem(i, 4, guidItem);
        ui->partitionsTable->setItem(i, 5, statusItem);
        ui->partitionsTable->setItem(i, 6, lbaItem);
    }

    // Сортировка по смещению
    ui->partitionsTable->sortByColumn(1, Qt::AscendingOrder);

    // Активация кнопок извлечения
    bool hasPartitions = partitions.size() > 0;
    ui->extractButton->setEnabled(hasPartitions);
    ui->extractAllButton->setEnabled(hasPartitions);

    // Показываем информационное сообщение
    QString message = QString("Анализ завершен. Найдено %1 разделов.").arg(partitions.size());
    logMessage(message);

    // Автопереключение на вкладку логов для просмотра деталей
    if (partitions.size() > 0) {
        ui->tabWidget->setCurrentIndex(5); // Вкладка логов
        logMessage("Для просмотра деталей анализа перейдите на вкладку 'Логи'");
    }
}

/**
 * @brief Форматирует размер файла в читаемый вид
 * @param size Размер в байтах
 * @return Отформатированная строка
 */
QString MainWindow::formatFileSize(qint64 size)
{
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = KB * 1024;
    constexpr qint64 GB = MB * 1024;
    constexpr qint64 TB = GB * 1024;

    if (size >= TB) {
        return QString("%1 ТБ").arg(size / (double)TB, 0, 'f', 2);
    } else if (size >= GB) {
        return QString("%1 ГБ").arg(size / (double)GB, 0, 'f', 2);
    } else if (size >= MB) {
        return QString("%1 МБ").arg(size / (double)MB, 0, 'f', 2);
    } else if (size >= KB) {
        return QString("%1 КБ").arg(size / (double)KB, 0, 'f', 2);
    } else {
        return QString("%1 Б").arg(size);
    }
}

/**
 * @brief Включает/отключает элементы интерфейса
 * @param enabled true для включения, false для отключения
 */
void MainWindow::setUiEnabled(bool enabled)
{
    ui->browseButton->setEnabled(enabled);
    ui->scanButton->setEnabled(enabled);
    ui->extractButton->setEnabled(enabled && ui->partitionsTable->rowCount() > 0);
    ui->extractAllButton->setEnabled(enabled && ui->partitionsTable->rowCount() > 0);

    // Обновление статуса
    if (enabled) {
        ui->statusbar->showMessage("Готов", 2000);
    } else {
        ui->statusbar->showMessage("Анализ в процессе...");
    }
}

/**
 * @brief Слот для обработки нажатия кнопки "Извлечь выбранное"
 */
void MainWindow::onExtractClicked()
{
    QList<QTableWidgetItem*> selectedItems = ui->partitionsTable->selectedItems();
    if (selectedItems.isEmpty()) {
        showError("Пожалуйста, выберите разделы для извлечения.");
        return;
    }

    QSet<int> selectedRows;
    for (auto item : selectedItems) {
        selectedRows.insert(item->row());
    }

    logMessage(QString("Запрошено извлечение %1 разделов").arg(selectedRows.size()));

    // Показываем диалог подтверждения
    QStringList partitionNames;
    for (int row : selectedRows) {
        QTableWidgetItem* nameItem = ui->partitionsTable->item(row, 0);
        if (nameItem) {
            partitionNames.append(nameItem->text());
        }
    }

    QString message = QString("Вы действительно хотите извлечь следующие разделы?\n\n%1\n\nФункция находится в разработке.")
                          .arg(partitionNames.join("\n"));

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Подтверждение", message,
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        logMessage("Извлечение разделов отменено (функция в разработке)");
        QMessageBox::information(this, "Информация", "Функция извлечения разделов находится в разработке.");
    }
}

/**
 * @brief Слот для обработки нажатия кнопки "Извлечь всё"
 */
void MainWindow::onExtractAllClicked()
{
    int rowCount = ui->partitionsTable->rowCount();
    if (rowCount == 0) {
        showError("Нет разделов для извлечения.");
        return;
    }

    logMessage(QString("Запрошено извлечение всех %1 разделов").arg(rowCount));

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Подтверждение",
                                                              QString("Вы действительно хотите извлечь все %1 разделов?\n\nФункция находится в разработке.").arg(rowCount),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        logMessage("Извлечение всех разделов отменено (функция в разработке)");
        QMessageBox::information(this, "Информация", "Функция извлечения разделов находится в разработке.");
    }
}

/**
 * @brief Слот для обработки нажатия кнопки "Обновить" в файловом менеджере
 */
void MainWindow::onRefreshFilesClicked()
{
    logMessage("Запрошено обновление списка файлов");
    QMessageBox::information(this, "Информация", "Функция сканирования файлов находится в разработке.");
}

/**
 * @brief Слот для обработки нажатия кнопки "Извлечь файл"
 */
void MainWindow::onExtractFileClicked()
{
    logMessage("Запрошено извлечение файлов");
    QMessageBox::information(this, "Информация", "Функция извлечения файлов находится в разработке.");
}

/**
 * @brief Слот для обработки нажатия кнопки "Поиск файлов"
 */
void MainWindow::onFindFilesClicked()
{
    logMessage("Запрошен поиск файлов");
    QMessageBox::information(this, "Информация", "Функция поиска файлов находится в разработке.");
}

/**
 * @brief Слот для обработки нажатия кнопки "Анализировать" суперблока
 */
void MainWindow::onAnalyzeSuperClicked()
{
    logMessage("Запрошен анализ суперблока");
    QMessageBox::information(this, "Информация", "Функция анализа суперблока находится в разработке.");
}

/**
 * @brief Слот для обработки нажатия кнопки "Обзор..." в настройках
 */
void MainWindow::onBrowseExtractClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    "Выберите папку для извлечения",
                                                    ui->extractPathEdit->text(),
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        ui->extractPathEdit->setText(dir);
        logMessage(QString("Выбрана папка для извлечения: %1").arg(dir));
    }
}

/**
 * @brief Слот для обработки нажатия кнопки "Сохранить настройки"
 */
void MainWindow::onSaveSettingsClicked()
{
    saveSettings();
    logMessage("Настройки сохранены");
    QMessageBox::information(this, "Успех", "Настройки успешно сохранены.");
}

/**
 * @brief Слот для обработки нажатия кнопки "По умолчанию"
 */
void MainWindow::onDefaultSettingsClicked()
{
    // Сброс настроек к значениям по умолчанию
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                          + "/AndroidExtractor";
    ui->extractPathEdit->setText(defaultPath);
    ui->saveLogsCheckBox->setChecked(true);
    ui->detailLevelCombo->setCurrentIndex(1);
    ui->autoScrollCheckBox->setChecked(true);

    logMessage("Настройки сброшены к значениям по умолчанию");
    QMessageBox::information(this, "Информация", "Настройки сброшены к значениям по умолчанию.");
}

/**
 * @brief Слот для обработки нажатия кнопки "Сохранить отчёт"
 */
void MainWindow::onSaveReportClicked()
{
    logMessage("Запрошено сохранение отчета");
    QMessageBox::information(this, "Информация", "Функция сохранения отчета находится в разработке.");
}

/**
 * @brief Показать контекстное меню для таблицы разделов
 * @param pos Позиция курсора в координатах виджета
 */
void MainWindow::showPartitionContextMenu(const QPoint& pos)
{
    QTableWidgetItem* item = ui->partitionsTable->itemAt(pos);
    if (item) {
        m_selectedPartitionRow = item->row();
        m_contextMenu->exec(ui->partitionsTable->viewport()->mapToGlobal(pos));
    }
}

/**
 * @brief Копировать имя раздела в буфер обмена
 */
void MainWindow::copyPartitionName()
{
    if (m_selectedPartitionRow >= 0 && m_selectedPartitionRow < ui->partitionsTable->rowCount()) {
        QTableWidgetItem* item = ui->partitionsTable->item(m_selectedPartitionRow, 0);
        if (item) {
            QApplication::clipboard()->setText(item->text());
            logMessage(QString("Скопировано имя раздела: %1").arg(item->text()));
            ui->statusbar->showMessage("Имя раздела скопировано в буфер обмена", 2000);
        }
    }
}

/**
 * @brief Копировать GUID раздела в буфер обмена
 */
void MainWindow::copyPartitionGuid()
{
    if (m_selectedPartitionRow >= 0 && m_selectedPartitionRow < ui->partitionsTable->rowCount()) {
        QTableWidgetItem* item = ui->partitionsTable->item(m_selectedPartitionRow, 4);
        if (item) {
            QApplication::clipboard()->setText(item->text());
            logMessage(QString("Скопирован GUID раздела: %1").arg(item->text()));
            ui->statusbar->showMessage("GUID раздела скопирован в буфер обмена", 2000);
        }
    }
}

/**
 * @brief Копировать смещение раздела в буфер обмена
 */
void MainWindow::copyPartitionOffset()
{
    if (m_selectedPartitionRow >= 0 && m_selectedPartitionRow < ui->partitionsTable->rowCount()) {
        QTableWidgetItem* item = ui->partitionsTable->item(m_selectedPartitionRow, 1);
        if (item) {
            QApplication::clipboard()->setText(item->text());
            logMessage(QString("Скопировано смещение раздела: %1").arg(item->text()));
            ui->statusbar->showMessage("Смещение раздела скопировано в буфер обмена", 2000);
        }
    }
}

/**
 * @brief Показать выбранный раздел в Hex Viewer
 */
void MainWindow::showInHexViewer()
{
    if (m_selectedPartitionRow >= 0 && m_selectedPartitionRow < ui->partitionsTable->rowCount()) {
        // Получаем информацию о разделе
        QTableWidgetItem* nameItem = ui->partitionsTable->item(m_selectedPartitionRow, 0);
        QTableWidgetItem* offsetItem = ui->partitionsTable->item(m_selectedPartitionRow, 1);
        QTableWidgetItem* sizeItem = ui->partitionsTable->item(m_selectedPartitionRow, 2);

        if (nameItem && offsetItem && sizeItem) {
            QString partitionName = nameItem->text();
            QString offsetStr = offsetItem->text();

            // Преобразуем смещение из hex строки в число
            bool ok;
            qint64 offset = offsetStr.mid(2).toLongLong(&ok, 16); // Пропускаем "0x"

            if (ok && m_hexViewer) {
                // Загружаем данные раздела в Hex Viewer
                QString filePath = ui->filePathEdit->text();
                if (!filePath.isEmpty() && QFile::exists(filePath)) {
                    // Читаем первые 4096 байт раздела для предпросмотра
                    if (m_hexViewer->loadDataFromFile(filePath, offset, 4096)) {
                        logMessage(QString("Загружен раздел '%1' в Hex Viewer (первые 4096 байт)").arg(partitionName));
                        ui->tabWidget->setCurrentIndex(1); // Переключаем на вкладку Hex Viewer
                    } else {
                        showError("Не удалось загрузить данные раздела в Hex Viewer");
                    }
                } else {
                    showError("Файл не выбран или не существует");
                }
            }
        }
    }
}

/**
 * @brief Обработка двойного клика по разделу в таблице
 * @param item Элемент таблицы, по которому был выполнен двойной клик
 */
void MainWindow::onPartitionDoubleClicked(QTableWidgetItem* item)
{
    if (item) {
        int row = item->row();
        QString partitionName = ui->partitionsTable->item(row, 0)->text();
        logMessage(QString("Двойной клик по разделу: %1 (строка %2)").arg(partitionName).arg(row));

        // Показываем информацию о разделе в Hex Viewer
        showInHexViewer();
    }
}

/**
 * @brief Обработка двойного клика по файлу в дереве
 * @param item Элемент дерева файлов
 * @param column Номер колонки
 */
void MainWindow::onFileDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (item) {
        QString fileName = item->text(0);
        logMessage(QString("Двойной клик по файлу: %1").arg(fileName));

        QMessageBox::information(this, "Информация",
                                 QString("Двойной клик по файлу '%1'.\nФункция просмотра файлов находится в разработке.").arg(fileName));
    }
}

/**
 * @brief Слот для обработки сообщений лога
 * @param message Сообщение лога
 */
void MainWindow::onLogMessage(const QString &message)
{
    // Добавляем сообщение в лог
    ui->logTextEdit->append(message);

    // Автопрокрутка, если включена
    if (ui->autoScrollCheckBox->isChecked()) {
        QTextCursor cursor = ui->logTextEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->logTextEdit->setTextCursor(cursor);
    }

    // Показываем в statusbar (первые 80 символов)
    QString shortMessage = message;
    if (shortMessage.length() > 80) {
        shortMessage = shortMessage.left(77) + "...";
    }
    ui->statusbar->showMessage(shortMessage, 3000);
}

/**
 * @brief Слот для обработки нажатия кнопки "Очистить лог"
 */
void MainWindow::onClearLogClicked()
{
    ui->logTextEdit->clear();
    QString timestamp = QDateTime::currentDateTime().toString("[HH:mm:ss]");
    ui->logTextEdit->append(timestamp + " Лог очищен");
    ui->statusbar->showMessage("Лог очищен", 2000);
}

/**
 * @brief Слот для обработки нажатия кнопки "Сохранить лог"
 */
void MainWindow::onSaveLogClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "Сохранить лог",
                                                    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
                                                        "/android_extractor_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss") + ".log",
                                                    "Лог файлы (*.log *.txt);;Все файлы (*.*)");

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << ui->logTextEdit->toPlainText();
            file.close();
            logMessage(QString("Лог сохранен в: %1").arg(fileName));
            ui->statusbar->showMessage("Лог сохранен", 3000);
        } else {
            showError("Не удалось сохранить лог");
        }
    }
}

/**
 * @brief Слот для обработки нажатия кнопки "Копировать лог"
 */
void MainWindow::onCopyLogClicked()
{
    QString logText = ui->logTextEdit->toPlainText();
    if (!logText.isEmpty()) {
        QApplication::clipboard()->setText(logText);
        logMessage("Лог скопирован в буфер обмена");
        ui->statusbar->showMessage("Лог скопирован в буфер обмена", 2000);
    } else {
        showError("Лог пуст");
    }
}

/**
 * @brief Слот для обработки изменения состояния чекбокса автопрокрутки
 * @param state Состояние чекбокса
 */
void MainWindow::onAutoScrollChanged(int state)
{
    Q_UNUSED(state);
    logMessage(QString("Автопрокрутка логов %1").arg(ui->autoScrollCheckBox->isChecked() ? "включена" : "выключена"));
}

/**
 * @brief Показать сообщение об ошибке
 * @param message Текст сообщения об ошибке
 */
void MainWindow::showError(const QString& message)
{
    logMessage("ОШИБКА: " + message);
    QMessageBox::critical(this, "Ошибка", message);
    ui->statusbar->showMessage("Ошибка: " + message, 5000);
}

