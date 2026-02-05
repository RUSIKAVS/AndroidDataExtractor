#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "partitionanalyzer.h"
#include "superanalyzer.h"
#include "guidmanager.h"
#include "filemanager.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTreeWidgetItem>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QInputDialog>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QApplication>
#include <QPalette>
#include <QMenu>
#include <QAction>
#include <QFileInfo>
#include <QIcon>
#include <QDir>
#include <QFileInfoList>
#include <QTimer>
#include <QDateTime>
#include <QTextCursor>

// Конструктор главного окна
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_partitionAnalyzer(std::make_unique<PartitionAnalyzer>())
    , m_superAnalyzer(std::make_unique<SuperAnalyzer>())
    , m_guidManager(std::make_unique<GuidManager>())
    , m_fileManager(std::make_unique<FileManager>())
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr)
{
    ui->setupUi(this);

    // Инициализация интерфейса
    initUI();

    // Установка темной темы по умолчанию
    applyDarkTheme();

    // Установка заголовка окна
    setWindowTitle("Android Data Extractor - Отладка");

    // Подключение сигналов прогресса
    connect(m_fileManager.get(), &FileManager::progressChanged,
            this, &MainWindow::onFileProgressChanged);
}

// Деструктор
MainWindow::~MainWindow()
{
    delete ui;
}

// Инициализация пользовательского интерфейса
void MainWindow::initUI()
{
    // Настройка дерева разделов
    setupTreeWidget();

    // Настройка таблицы GUID
    setupGuidTable();

    // Настройка hex-просмотрщика
    setupHexViewer();

    // Настройка логов
    setupLogsViewer();

    // Настройка статусной строки
    setupStatusBar();

    // Настройка меню и панели инструментов
    setupMenuAndToolbar();

    // Подключение сигналов и слотов
    connectSignalsSlots();
}

// Настройка виджета дерева разделов
void MainWindow::setupTreeWidget()
{
    // Установка количества колонок и заголовков
    ui->treeWidget->setColumnCount(4);
    ui->treeWidget->setHeaderLabels({"Имя", "Размер", "GUID", "Тип"});

    // Настройка отображения колонок
    ui->treeWidget->setAlternatingRowColors(true);
    ui->treeWidget->setAnimated(true);

    // Настройка поведения заголовков
    QHeaderView* header = ui->treeWidget->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Stretch);    // Имя - растягиваем
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents); // Размер - по содержимому
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents); // GUID - по содержимому
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Тип - по содержимому

    // Включение контекстного меню
    ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
}

// Настройка таблицы GUID
void MainWindow::setupGuidTable()
{
    // Установка количества колонок и заголовков
    ui->guidTableWidget->setColumnCount(4);
    ui->guidTableWidget->setHorizontalHeaderLabels({"GUID", "Тип", "Описание", "Раздел"});

    // Настройка отображения
    ui->guidTableWidget->setAlternatingRowColors(true);
    ui->guidTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->guidTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->guidTableWidget->setShowGrid(true);

    // Настройка поведения заголовков
    ui->guidTableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->guidTableWidget->verticalHeader()->setVisible(false);
}

// Настройка hex-просмотрщика (упрощенная версия)
void MainWindow::setupHexViewer()
{
    QFont font("Courier New", 10);
    ui->hexViewer->setFont(font);
    ui->hexViewer->setReadOnly(true);
    ui->hexViewer->setLineWrapMode(QPlainTextEdit::NoWrap);
}

// Настройка логов
void MainWindow::setupLogsViewer()
{
    QFont font("Courier New", 9);
    ui->logsTextEdit->setFont(font);
    ui->logsTextEdit->setReadOnly(true);
    ui->logsTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
}

// Настройка статусной строки
void MainWindow::setupStatusBar()
{
    // Создание постоянных виджетов в статусной строке
    m_statusLabel = new QLabel("Готово");
    ui->statusbar->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumWidth(200);
    ui->statusbar->addPermanentWidget(m_progressBar);
}

// Настройка меню и панели инструментов (упрощенная версия)
void MainWindow::setupMenuAndToolbar()
{
    // Используем системные иконки Qt
    QIcon openIcon = QIcon::fromTheme("document-open");
    QIcon folderIcon = QIcon::fromTheme("folder-open");
    QIcon extractIcon = QIcon::fromTheme("document-save-as");
    QIcon analyzeIcon = QIcon::fromTheme("edit-find");
    QIcon exitIcon = QIcon::fromTheme("application-exit");
    QIcon darkIcon = QIcon::fromTheme("weather-clear-night");
    QIcon lightIcon = QIcon::fromTheme("weather-clear");
    QIcon aboutIcon = QIcon::fromTheme("help-about");

    // Настраиваем иконки действий
    ui->actionOpenImage->setIcon(openIcon);
    ui->actionOpenFolder->setIcon(folderIcon);
    ui->actionExtractPartition->setIcon(extractIcon);
    ui->actionAnalyzeSuper->setIcon(analyzeIcon);
    ui->actionExit->setIcon(exitIcon);
    ui->actionDarkTheme->setIcon(darkIcon);
    ui->actionLightTheme->setIcon(lightIcon);
    ui->actionAbout->setIcon(aboutIcon);

    // Иконки для кнопок логов (если они есть в UI)
    if (ui->clearLogsButton) {
        ui->clearLogsButton->setIcon(QIcon::fromTheme("edit-clear"));
    }
    if (ui->saveLogsButton) {
        ui->saveLogsButton->setIcon(QIcon::fromTheme("document-save"));
    }
}

// Подключение сигналов и слотов
void MainWindow::connectSignalsSlots()
{
    // Подключение сигналов дерева разделов
    connect(ui->treeWidget, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::on_treeWidget_itemDoubleClicked);

    connect(ui->treeWidget, &QTreeWidget::currentItemChanged,
            this, &MainWindow::on_treeWidget_currentItemChanged);

    connect(ui->treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::on_treeWidget_customContextMenuRequested);

    // Подключение сигналов таблицы GUID
    connect(ui->guidTableWidget, &QTableWidget::itemDoubleClicked,
            this, &MainWindow::on_guidTableWidget_itemDoubleClicked);

    // Подключение кнопок логов
    connect(ui->clearLogsButton, &QPushButton::clicked,
            this, &MainWindow::on_clearLogsButton_clicked);
    connect(ui->saveLogsButton, &QPushButton::clicked,
            this, &::MainWindow::on_saveLogsButton_clicked);
}

// Применение темной темы
void MainWindow::applyDarkTheme()
{
    // Создание темной палитры
    QPalette darkPalette;

    // Настройка основных цветов
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    // Настройка цветов для отключенных элементов
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));

    // Применение палитры ко всему приложению
    qApp->setPalette(darkPalette);

    // Установка стиля Fusion
    qApp->setStyle(QStyleFactory::create("Fusion"));

    // Дополнительные стили для виджетов
    QString styleSheet = R"(
        QTreeWidget {
            background-color: #2d2d2d;
            color: #ffffff;
            border: 1px solid #555555;
            alternate-background-color: #353535;
        }
        QTreeWidget::item {
            padding: 4px;
        }
        QTreeWidget::item:hover {
            background-color: #3a3a3a;
        }
        QTreeWidget::item:selected {
            background-color: #2a82da;
            color: #ffffff;
        }
        QTableWidget {
            background-color: #2d2d2d;
            color: #ffffff;
            gridline-color: #555555;
            alternate-background-color: #353535;
        }
        QTableWidget::item:hover {
            background-color: #3a3a3a;
        }
        QTableWidget::item:selected {
            background-color: #2a82da;
            color: #ffffff;
        }
        QHeaderView::section {
            background-color: #2b2b2b;
            color: #ffffff;
            padding: 5px;
            border: 1px solid #555555;
        }
        QStatusBar {
            background-color: #2b2b2b;
            color: #ffffff;
        }
        QMenuBar {
            background-color: #2b2b2b;
            color: #ffffff;
        }
        QMenuBar::item:selected {
            background-color: #3a3a3a;
        }
        QMenu {
            background-color: #2d2d2d;
            color: #ffffff;
            border: 1px solid #555555;
        }
        QMenu::item:selected {
            background-color: #2a82da;
        }
        QToolBar {
            background-color: #2b2b2b;
            border: none;
            spacing: 3px;
        }
        QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            padding: 3px;
        }
        QToolButton:hover {
            background-color: #3a3a3a;
            border: 1px solid #555555;
        }
        QTabWidget::pane {
            border: 1px solid #555555;
            background-color: #2d2d2d;
        }
        QTabBar::tab {
            background-color: #353535;
            color: #ffffff;
            padding: 8px 16px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #2a82da;
        }
        QTabBar::tab:hover:!selected {
            background-color: #3a3a3a;
        }
        QPlainTextEdit {
            background-color: #1e1e1e;
            color: #ffffff;
            border: 1px solid #555555;
            font-family: 'Courier New';
        }
        QPushButton {
            background-color: #3a3a3a;
            color: #ffffff;
            border: 1px solid #555555;
            padding: 5px 10px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
        }
        QCheckBox {
            color: #ffffff;
        }
        QComboBox {
            background-color: #3a3a3a;
            color: #ffffff;
            border: 1px solid #555555;
        }
        QLineEdit {
            background-color: #3a3a3a;
            color: #ffffff;
            border: 1px solid #555555;
        }
    )";

    qApp->setStyleSheet(styleSheet);
}

// Применение светлой темы
void MainWindow::applyLightTheme()
{
    // Сброс стилей и применение стандартной палитры
    qApp->setStyleSheet("");
    qApp->setPalette(style()->standardPalette());
    qApp->setStyle(QStyleFactory::create("Fusion"));
}

// Добавить сообщение в лог
void MainWindow::logMessage(const QString &message, const QString &type)
{
    if (!ui->logsTextEdit) {
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formattedMessage;

    if (type == "error") {
        formattedMessage = QString("[%1] <font color='red'><b>ОШИБКА:</b> %2</font>").arg(timestamp, message);
    } else if (type == "warning") {
        formattedMessage = QString("[%1] <font color='orange'><b>ПРЕДУПРЕЖДЕНИЕ:</b> %2</font>").arg(timestamp, message);
    } else if (type == "success") {
        formattedMessage = QString("[%1] <font color='green'><b>УСПЕХ:</b> %2</font>").arg(timestamp, message);
    } else {
        formattedMessage = QString("[%1] %2").arg(timestamp, message);
    }

    // Добавляем HTML
    ui->logsTextEdit->appendHtml(formattedMessage);

    // Автопрокрутка если включена
    if (ui->autoScrollCheckBox && ui->autoScrollCheckBox->isChecked()) {
        QTextCursor cursor = ui->logsTextEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->logsTextEdit->setTextCursor(cursor);
    }
}

// Обновление статусной строки
void MainWindow::updateStatusBar(const QString& message, int timeout)
{
    if (m_statusLabel) {
        m_statusLabel->setText(message);
    }
    if (timeout > 0) {
        ui->statusbar->showMessage(message, timeout);
    }
}

// Показать/скрыть прогресс
void MainWindow::showProgress(bool show, int maximum)
{
    if (m_progressBar) {
        m_progressBar->setVisible(show);
        m_progressBar->setMaximum(maximum);
        if (!show) {
            m_progressBar->setValue(0);
        }
    }
}

// Обновить прогресс
void MainWindow::updateProgress(int value)
{
    if (m_progressBar && m_progressBar->isVisible()) {
        m_progressBar->setValue(value);
    }
}

// Слот для очистки логов
void MainWindow::on_clearLogsButton_clicked()
{
    if (ui->logsTextEdit) {
        ui->logsTextEdit->clear();
        logMessage("Логи очищены", "info");
    }
}

// Слот для сохранения логов
void MainWindow::on_saveLogsButton_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "Сохранить логи",
                                                    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/android_extractor_logs.txt",
                                                    "Текстовые файлы (*.txt);;Все файлы (*)");

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << ui->logsTextEdit->toPlainText();
            file.close();
            logMessage(QString("Логи сохранены в: %1").arg(fileName), "success");
            QMessageBox::information(this, "Успех", "Логи успешно сохранены.");
        } else {
            logMessage(QString("Не удалось сохранить логи в: %1").arg(fileName), "error");
            QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл.");
        }
    }
}

// Добавить раздел в дерево
void MainWindow::addPartitionToTree(const QString& name, const QString& guid,
                                    quint64 size, const QString& type,
                                    const QString& path, quint64 offset)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(ui->treeWidget);

    // Установка текста в колонках
    item->setText(0, name);
    item->setText(1, formatSize(size));
    item->setText(2, guid);
    item->setText(3, type);

    // Установка иконки в зависимости от типа раздела
    if (type.contains("super", Qt::CaseInsensitive) || name.contains("super", Qt::CaseInsensitive)) {
        item->setIcon(0, QIcon::fromTheme("drive-harddisk"));
        item->setForeground(0, QBrush(QColor(255, 165, 0))); // Оранжевый для super
    } else if (type.contains("linux", Qt::CaseInsensitive) || name.contains("system", Qt::CaseInsensitive)) {
        item->setIcon(0, QIcon::fromTheme("drive-harddisk"));
    } else if (type.contains("fat", Qt::CaseInsensitive) || name.contains("boot", Qt::CaseInsensitive)) {
        item->setIcon(0, QIcon::fromTheme("drive-harddisk"));
    } else if (name.contains("userdata", Qt::CaseInsensitive) || name.contains("data", Qt::CaseInsensitive)) {
        item->setIcon(0, QIcon::fromTheme("drive-harddisk"));
    } else {
        item->setIcon(0, QIcon::fromTheme("drive-harddisk"));
    }

    // Сохранение данных раздела
    QVariantMap partData;
    partData["type"] = "partition";
    partData["name"] = name;
    partData["guid"] = guid;
    partData["size"] = size;
    partData["path"] = path;
    partData["offset"] = offset;
    partData["partition_type"] = type;

    // Сохранение данных в элементе дерева
    item->setData(0, Qt::UserRole, partData);

    // Сохранение в карту разделов
    QString key = QString("%1@%2").arg(name).arg(offset);
    m_partitionMap[key] = partData;
}

// Форматирование размера
QString MainWindow::formatSize(quint64 bytes) const
{
    const QStringList units = {"Б", "КБ", "МБ", "ГБ", "ТБ"};
    double size = bytes;
    int unitIndex = 0;

    while (size >= 1024 && unitIndex < units.size() - 1) {
        size /= 1024;
        unitIndex++;
    }

    return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unitIndex]);
}

// Анализ файла образа
void MainWindow::analyzeImageFile(const QString& filePath)
{
    // Проверяем, не анализируем ли мы уже этот файл
    if (m_currentImagePath == filePath && !m_partitionMap.isEmpty()) {
        qDebug() << "Файл уже анализируется:" << filePath;
        return;
    }

    // Очистка предыдущих данных
    ui->treeWidget->clear();
    m_partitionMap.clear();
    m_currentImagePath = filePath;
    m_currentFolderPath.clear(); // Сбрасываем папку

    updateStatusBar("Анализ образа...");
    showProgress(true, 0);

    // Обработка событий для обновления UI
    QApplication::processEvents();

    try {
        if (m_partitionAnalyzer->analyzePartitions(filePath)) {
            auto partitions = m_partitionAnalyzer->getPartitions();

            // Добавление файла как корневого элемента
            QTreeWidgetItem* fileItem = new QTreeWidgetItem(ui->treeWidget);
            QFileInfo fileInfo(filePath);
            fileItem->setText(0, fileInfo.fileName());
            fileItem->setText(1, formatSize(fileInfo.size()));
            fileItem->setText(2, "");
            fileItem->setText(3, "Disk Image");
            fileItem->setIcon(0, QIcon::fromTheme("drive-harddisk"));
            fileItem->setExpanded(true);

            QVariantMap fileData;
            fileData["type"] = "disk_image";
            fileData["path"] = filePath;
            fileData["size"] = fileInfo.size();
            fileItem->setData(0, Qt::UserRole, fileData);

            // Добавление разделов как дочерних элементов
            for (const auto& partition : partitions) {
                addPartitionToTree(
                    partition.name,
                    partition.guid,
                    partition.size,
                    partition.type,
                    filePath,
                    partition.offset
                    );
            }

            // Проверка на наличие super раздела
            checkAndAddSuperPartition(filePath);

            updateStatusBar(QString("Анализ завершен. Разделов: %1").arg(partitions.size()), 3000);

            // Автоматический анализ super раздела, если найден
            autoSuperPartitionAnalysis();
        } else {
            // Если не удалось распарсить как образ, возможно это просто файл
            QTreeWidgetItem* fileItem = new QTreeWidgetItem(ui->treeWidget);
            QFileInfo fileInfo(filePath);
            fileItem->setText(0, fileInfo.fileName());
            fileItem->setText(1, formatSize(fileInfo.size()));
            fileItem->setText(2, "");
            fileItem->setText(3, "File");
            fileItem->setIcon(0, QIcon::fromTheme("text-x-generic"));

            QVariantMap fileData;
            fileData["type"] = "file";
            fileData["path"] = filePath;
            fileData["size"] = fileInfo.size();
            fileItem->setData(0, Qt::UserRole, fileData);

            // Проверяем, не является ли это super разделом
            if (SuperAnalyzer::isSuperImage(filePath)) {
                fileItem->setText(3, "SUPER Image");
                fileItem->setIcon(0, QIcon::fromTheme("drive-harddisk"));
                fileData["is_super"] = true;
                fileItem->setData(0, Qt::UserRole, fileData);

                // Предлагаем анализ асинхронно
                QTimer::singleShot(100, this, [this]() {
                    int result = QMessageBox::question(this, "SUPER раздел",
                                                       "Обнаружен SUPER раздел.\nВыполнить анализ?",
                                                       QMessageBox::Yes | QMessageBox::No,
                                                       QMessageBox::Yes);
                    if (result == QMessageBox::Yes) {
                        on_actionAnalyzeSuper_triggered();
                    }
                });
            }

            updateStatusBar("Файл открыт (не является образом диска)", 3000);
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Критическая ошибка",
                              QString("Исключение при анализе: %1").arg(e.what()));
        updateStatusBar("Критическая ошибка при анализе", 3000);
    } catch (...) {
        QMessageBox::critical(this, "Критическая ошибка",
                              "Неизвестное исключение при анализе файла");
        updateStatusBar("Неизвестная ошибка при анализе", 3000);
    }

    showProgress(false);
}


// Проверка и добавление super раздела
void MainWindow::checkAndAddSuperPartition(const QString& filePath)
{
    // Поиск super раздела среди обычных разделов
    auto partitions = m_partitionAnalyzer->getPartitions();

    for (const auto& partition : partitions) {
        if (partition.name.contains("super", Qt::CaseInsensitive) ||
            partition.type.contains("super", Qt::CaseInsensitive) ||
            partition.guid.contains("E6A98E58", Qt::CaseInsensitive)) { // GUID динамического раздела

            // Добавление super раздела в дерево
            QTreeWidgetItem* superItem = new QTreeWidgetItem(ui->treeWidget);
            superItem->setText(0, QString("SUPER [%1]").arg(partition.name));
            superItem->setText(1, formatSize(partition.size));
            superItem->setText(2, partition.guid);
            superItem->setText(3, "Dynamic Super Partition");
            superItem->setIcon(0, QIcon::fromTheme("drive-harddisk"));
            superItem->setForeground(0, QBrush(QColor(255, 165, 0))); // Оранжевый

            // Сохранение данных super раздела
            QVariantMap superData;
            superData["type"] = "super_container";
            superData["name"] = partition.name;
            superData["path"] = filePath;
            superData["offset"] = partition.offset;
            superData["size"] = partition.size;
            superData["guid"] = partition.guid;
            superItem->setData(0, Qt::UserRole, superData);

            break;
        }
    }
}

// Автоматический анализ super раздела
void MainWindow::autoSuperPartitionAnalysis()
{
    // Поиск super раздела в дереве
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = ui->treeWidget->topLevelItem(i);
        QVariant data = item->data(0, Qt::UserRole);

        if (data.isValid() && data.typeId() == QMetaType::QVariantMap) {
            QVariantMap map = data.toMap();
            if (map["type"].toString() == "super_container") {
                // Найден super раздел, можно предложить анализ
                int result = QMessageBox::question(this, "Обнаружен SUPER раздел",
                                                   "Обнаружен динамический раздел SUPER.\n"
                                                   "Выполнить автоматический анализ?",
                                                   QMessageBox::Yes | QMessageBox::No,
                                                   QMessageBox::Yes);

                if (result == QMessageBox::Yes) {
                    on_actionAnalyzeSuper_triggered();
                }
                break;
            }
        }
    }
}

// Анализ папки
void MainWindow::analyzeFolder(const QString& folderPath)
{
    m_currentFolderPath = folderPath;
    m_currentImagePath.clear(); // Сбрасываем файл
    ui->treeWidget->clear();
    m_partitionMap.clear();

    updateStatusBar(QString("Анализ папки: %1").arg(folderPath));
    showProgress(true, 0);

    QApplication::processEvents();

    try {
        // Сканируем папку на наличие образов
        scanFolderForImages(folderPath);

        updateStatusBar(QString("Анализ папки завершен. Файлов найдено: %1")
                            .arg(ui->treeWidget->topLevelItemCount()), 3000);

    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Ошибка",
                              QString("Исключение при анализе папки: %1").arg(e.what()));
        updateStatusBar("Критическая ошибка при анализе папки", 3000);
    } catch (...) {
        QMessageBox::critical(this, "Ошибка",
                              "Неизвестное исключение при анализе папки");
        updateStatusBar("Неизвестная ошибка при анализе папки", 3000);
    }

    showProgress(false);
}

// Сканирование папки на наличие образов
void MainWindow::scanFolderForImages(const QString& folderPath)
{
    QDir dir(folderPath);

    // Устанавливаем фильтры для поиска образов
    QStringList filters;
    filters << "*.img" << "*.bin" << "*.raw" << "*.super"
            << "*.sparseimg" << "*.mbn" << "*.sin" << "*.dmp";

    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::NoSymLinks | QDir::Readable);

    QFileInfoList fileList = dir.entryInfoList();

    // Добавляем саму папку как корневой элемент
    QTreeWidgetItem* folderItem = new QTreeWidgetItem(ui->treeWidget);
    folderItem->setText(0, QFileInfo(folderPath).fileName());
    folderItem->setText(1, "Папка");
    folderItem->setText(2, "");
    folderItem->setText(3, "Directory");
    folderItem->setIcon(0, QIcon::fromTheme("folder"));
    folderItem->setExpanded(true);

    // Сохраняем данные папки
    QVariantMap folderData;
    folderData["type"] = "folder";
    folderData["path"] = folderPath;
    folderItem->setData(0, Qt::UserRole, folderData);

    // Анализируем каждый файл
    for (const QFileInfo& fileInfo : fileList) {
        QString filePath = fileInfo.absoluteFilePath();

        // Проверяем, является ли файл образом диска или super разделом
        bool isDiskImage = PartitionAnalyzer::isDiskImage(filePath);
        bool isSuperImage = SuperAnalyzer::isSuperImage(filePath);

        if (isDiskImage || isSuperImage) {
            QTreeWidgetItem* fileItem = new QTreeWidgetItem(folderItem);
            fileItem->setText(0, fileInfo.fileName());
            fileItem->setText(1, formatSize(fileInfo.size()));
            fileItem->setText(2, "");
            fileItem->setText(3, isSuperImage ? "SUPER Image" : "Disk Image");
            fileItem->setIcon(0, QIcon::fromTheme("drive-harddisk"));

            // Сохраняем данные файла
            QVariantMap fileData;
            fileData["type"] = "file";
            fileData["path"] = filePath;
            fileData["is_super"] = isSuperImage;
            fileData["is_disk_image"] = isDiskImage;
            fileData["size"] = fileInfo.size();
            fileItem->setData(0, Qt::UserRole, fileData);
        }
    }

    // Если файлов не найдено, добавляем сообщение
    if (folderItem->childCount() == 0) {
        QTreeWidgetItem* noFilesItem = new QTreeWidgetItem(folderItem);
        noFilesItem->setText(0, "Образы не найдены");
        noFilesItem->setText(1, "");
        noFilesItem->setText(2, "");
        noFilesItem->setText(3, "No images found");
        noFilesItem->setIcon(0, QIcon::fromTheme("dialog-warning"));
        noFilesItem->setForeground(0, QBrush(Qt::gray));
    }
}

// Показать информацию о разделе
void MainWindow::showPartitionInfo(const QVariantMap& partData)
{
    QString info = QString("Информация о разделе:\n\n"
                           "Имя: %1\n"
                           "Тип: %2\n"
                           "GUID: %3\n"
                           "Размер: %4\n"
                           "Смещение: 0x%5\n"
                           "Путь: %6")
                       .arg(partData["name"].toString())
                       .arg(partData.value("partition_type", "N/A").toString())
                       .arg(partData["guid"].toString())
                       .arg(formatSize(partData["size"].toULongLong()))
                       .arg(partData["offset"].toULongLong(), 0, 16)
                       .arg(partData["path"].toString());

    QMessageBox::information(this, "Информация о разделе", info);
}

// ==================== СЛОТЫ ДЛЯ МЕНЮ ====================

// Открытие образа диска
void MainWindow::on_actionOpenImage_triggered()
{
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "Открыть образ диска",
                                                    QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                                    "Образы дисков (*.img *.bin *.raw *.super *.sparseimg);;"
                                                    "Все файлы (*)");

    if (!filePath.isEmpty()) {
        analyzeImageFile(filePath);
    }
}

// Открытие папки для анализа
void MainWindow::on_actionOpenFolder_triggered()
{
    QString folderPath = QFileDialog::getExistingDirectory(this,
                                                           "Открыть папку для анализа",
                                                           QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                                           QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!folderPath.isEmpty()) {
        analyzeFolder(folderPath);
    }
}

// Анализ SUPER раздела
void MainWindow::on_actionAnalyzeSuper_triggered()
{
    if (m_currentImagePath.isEmpty() && m_currentFolderPath.isEmpty()) {
        QMessageBox::information(this, "Информация",
                                 "Сначала откройте образ или папку.");
        return;
    }

    updateStatusBar("Анализ SUPER раздела...");
    showProgress(true, 0);

    QApplication::processEvents();

    try {
        // Проверяем, выбран ли конкретный super раздел в дереве
        QTreeWidgetItem* currentItem = ui->treeWidget->currentItem();
        QString superPath = m_currentImagePath;
        quint64 superOffset = 0;

        if (currentItem) {
            QVariant data = currentItem->data(0, Qt::UserRole);
            if (data.isValid() && data.typeId() == QMetaType::QVariantMap) {
                QVariantMap map = data.toMap();
                if (map["type"].toString() == "super_container") {
                    superPath = map["path"].toString();
                    superOffset = map["offset"].toULongLong();
                    qDebug() << "Анализ конкретного SUPER раздела, offset:" << superOffset;
                } else if (map["type"].toString() == "file" && map["is_super"].toBool()) {
                    superPath = map["path"].toString();
                    qDebug() << "Анализ файла SUPER раздела:" << superPath;
                }
            }
        }

        if (superPath.isEmpty()) {
            QMessageBox::warning(this, "Ошибка",
                                 "Не выбран SUPER раздел для анализа.");
            showProgress(false);
            return;
        }

        // Анализ super раздела с учетом смещения
        bool success = m_superAnalyzer->analyzeSuper(superPath, superOffset);

        if (success) {
            // Добавляем разделы SUPER в дерево
            m_superAnalyzer->populateTreeWidget(ui->treeWidget);

            // Показываем GUID в таблице
            auto partitions = m_superAnalyzer->getPartitions();
            ui->guidTableWidget->setRowCount(0);

            for (const auto& partition : partitions) {
                int row = ui->guidTableWidget->rowCount();
                ui->guidTableWidget->insertRow(row);

                ui->guidTableWidget->setItem(row, 0, new QTableWidgetItem(partition.guid));
                ui->guidTableWidget->setItem(row, 1, new QTableWidgetItem("Dynamic"));
                ui->guidTableWidget->setItem(row, 2,
                                             new QTableWidgetItem(m_guidManager->getGuidDescription(partition.guid)));
                ui->guidTableWidget->setItem(row, 3, new QTableWidgetItem(partition.name));
            }

            updateStatusBar(QString("SUPER проанализирован. Разделов: %1").arg(partitions.size()), 3000);
        } else {
            QMessageBox::warning(this, "Ошибка",
                                 "Не удалось проанализировать SUPER раздел.\n"
                                 "Возможно, он поврежден или имеет неизвестный формат.");
            updateStatusBar("Ошибка анализа SUPER", 3000);
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Ошибка",
                              QString("Исключение при анализе SUPER: %1").arg(e.what()));
        updateStatusBar("Критическая ошибка при анализе SUPER", 3000);
    } catch (...) {
        QMessageBox::critical(this, "Ошибка",
                              "Неизвестное исключение при анализе SUPER");
        updateStatusBar("Неизвестная ошибка при анализе SUPER", 3000);
    }

    showProgress(false);
}

// Извлечение раздела
void MainWindow::on_actionExtractPartition_triggered()
{
    QTreeWidgetItem* currentItem = ui->treeWidget->currentItem();
    if (!currentItem) {
        QMessageBox::information(this, "Информация",
                                 "Выберите раздел для извлечения.");
        return;
    }

    // Получение данных раздела
    QVariant data = currentItem->data(0, Qt::UserRole);
    if (!data.isValid() || data.typeId() != QMetaType::QVariantMap) {
        QMessageBox::warning(this, "Ошибка",
                             "Нет данных о разделе.");
        return;
    }

    QVariantMap partData = data.toMap();
    QString partType = partData["type"].toString();

    // Запрос пути для сохранения
    QString defaultName = partData["name"].toString().replace("/", "_") + ".img";
    QString savePath = QFileDialog::getSaveFileName(this,
                                                    "Сохранить раздел",
                                                    QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + defaultName,
                                                    "Образы дисков (*.img *.bin);;Все файлы (*)");

    if (savePath.isEmpty()) {
        return;
    }

    updateStatusBar("Извлечение раздела...");
    showProgress(true, 100);

    QApplication::processEvents();

    try {
        bool success = false;

        if (partType == "super_partition") {
            // Извлечение раздела из SUPER
            success = m_superAnalyzer->extractPartition(
                partData["name"].toString(),
                savePath
                );
        } else if (partType == "partition") {
            // Извлечение обычного раздела
            success = m_fileManager->extractPartition(
                partData["path"].toString(),
                partData["offset"].toULongLong(),
                partData["size"].toULongLong(),
                savePath
                );
        } else {
            QMessageBox::warning(this, "Ошибка",
                                 "Неподдерживаемый тип раздела.");
            showProgress(false);
            return;
        }

        if (success) {
            updateStatusBar(QString("Раздел успешно извлечен в: %1").arg(savePath), 5000);
            QMessageBox::information(this, "Успех",
                                     QString("Раздел успешно извлечен в:\n%1")
                                         .arg(savePath));
        } else {
            QMessageBox::warning(this, "Ошибка",
                                 "Не удалось извлечь раздел.");
            updateStatusBar("Ошибка извлечения раздела", 3000);
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Критическая ошибка",
                              QString("Исключение при извлечении: %1").arg(e.what()));
        updateStatusBar("Критическая ошибка при извлечении", 3000);
    } catch (...) {
        QMessageBox::critical(this, "Критическая ошибка",
                              "Неизвестное исключение при извлечении");
        updateStatusBar("Неизвестная ошибка при извлечении", 3000);
    }

    showProgress(false);
}

// Переключение на темную тему
void MainWindow::on_actionDarkTheme_triggered()
{
    applyDarkTheme();
    updateStatusBar("Темная тема активирована", 2000);
}

// Переключение на светлую тему
void MainWindow::on_actionLightTheme_triggered()
{
    applyLightTheme();
    updateStatusBar("Светлая тема активирована", 2000);
}

// Выход из приложения
void MainWindow::on_actionExit_triggered()
{
    close();
}

// О программе
void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, "О программе",
                       "Android Data Extractor\n"
                       "Версия: 1.2.0\n\n"
                       "Инструмент для анализа и извлечения данных\n"
                       "с Android устройств и образов дисков.\n\n"
                       "Поддержка:\n"
                       "- Обычных разделов (MBR/GPT)\n"
                       "- Динамических разделов SUPER\n"
                       "- Анализа GUID\n"
                       "- Работы с папками и файлами\n"
                       "- Темной и светлой темы\n\n"
                       "Требования: C++20, Qt6\n"
                       "Лицензия: MIT");
}

// ==================== СЛОТЫ ДЛЯ ВИДЖЕТОВ ====================

// Двойной клик по элементу дерева
void MainWindow::on_treeWidget_itemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    if (!item) {
        return;
    }

    // Получение данных элемента
    QVariant data = item->data(0, Qt::UserRole);
    if (!data.isValid()) {
        return;
    }

    if (data.typeId() == QMetaType::QVariantMap) {
        QVariantMap map = data.toMap();
        QString type = map["type"].toString();

        qDebug() << "Двойной клик по элементу типа:" << type;

        if (type == "disk_image" || type == "file") {
            // Открыть файл для анализа
            QString filePath = map["path"].toString();
            bool isSuper = map.value("is_super", false).toBool();

            qDebug() << "Файл:" << filePath << "isSuper:" << isSuper;

            if (isSuper) {
                // Анализ SUPER раздела - запускаем асинхронно
                m_currentImagePath = filePath;
                QTimer::singleShot(0, this, [this]() {
                    on_actionAnalyzeSuper_triggered();
                });
            } else {
                // Анализ как обычного образа - запускаем асинхронно
                QTimer::singleShot(0, this, [this, filePath]() {
                    analyzeImageFile(filePath);
                });
            }
        }
        else if (type == "folder") {
            // Открыть папку - запускаем асинхронно
            QString folderPath = map["path"].toString();
            QTimer::singleShot(0, this, [this, folderPath]() {
                analyzeFolder(folderPath);
            });
        }
        else if (type == "super_container") {
            // Анализ SUPER раздела - запускаем асинхронно
            int result = QMessageBox::question(this, "Анализ SUPER",
                                               "Выполнить анализ этого SUPER раздела?",
                                               QMessageBox::Yes | QMessageBox::No,
                                               QMessageBox::Yes);
            if (result == QMessageBox::Yes) {
                QTimer::singleShot(0, this, [this]() {
                    on_actionAnalyzeSuper_triggered();
                });
            }
        }
        else if (type == "super_partition") {
            // Показать информацию о разделе из SUPER
            QString guid = map["guid"].toString();
            QString name = map["name"].toString();

            ui->guidTableWidget->setRowCount(0);
            int row = ui->guidTableWidget->rowCount();
            ui->guidTableWidget->insertRow(row);

            ui->guidTableWidget->setItem(row, 0, new QTableWidgetItem(guid));
            ui->guidTableWidget->setItem(row, 1, new QTableWidgetItem("Dynamic"));
            ui->guidTableWidget->setItem(row, 2,
                                         new QTableWidgetItem(m_guidManager->getGuidDescription(guid)));
            ui->guidTableWidget->setItem(row, 3, new QTableWidgetItem(name));

            // Выделение вкладки с GUID
            ui->tabWidget->setCurrentIndex(1);

            updateStatusBar(QString("Информация о разделе: %1").arg(name), 2000);
        }
        else if (type == "partition") {
            // Показать информацию об обычном разделе
            QString guid = map["guid"].toString();
            QString name = map["name"].toString();

            ui->guidTableWidget->setRowCount(0);
            int row = ui->guidTableWidget->rowCount();
            ui->guidTableWidget->insertRow(row);

            ui->guidTableWidget->setItem(row, 0, new QTableWidgetItem(guid));
            ui->guidTableWidget->setItem(row, 1, new QTableWidgetItem("Partition"));
            ui->guidTableWidget->setItem(row, 2,
                                         new QTableWidgetItem(m_guidManager->getGuidDescription(guid)));
            ui->guidTableWidget->setItem(row, 3, new QTableWidgetItem(name));

            ui->tabWidget->setCurrentIndex(1); // Перейти на вкладку GUID
            updateStatusBar(QString("Раздел: %1").arg(name), 2000);
        }
    }
}


// Изменение текущего элемента дерева
void MainWindow::on_treeWidget_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    Q_UNUSED(previous);

    if (!current) {
        return;
    }

    // Получение данных текущего элемента
    QVariant data = current->data(0, Qt::UserRole);
    if (data.isValid() && data.typeId() == QMetaType::QVariantMap) {
        QVariantMap map = data.toMap();

        // Обновление информации в статусной строке
        if (map.contains("name")) {
            QString type = map["type"].toString();
            QString name = map["name"].toString();
            QString info;

            if (type == "super_container") {
                info = QString("SUPER раздел: %1").arg(name);
            } else if (type == "super_partition") {
                info = QString("Динамический раздел: %1").arg(name);
            } else if (type == "partition") {
                info = QString("Раздел: %1 (%2)").arg(name).arg(map["partition_type"].toString());
            } else if (type == "disk_image") {
                info = QString("Образ диска: %1").arg(name);
            } else if (type == "file") {
                info = QString("Файл: %1").arg(name);
            } else if (type == "folder") {
                info = QString("Папка: %1").arg(name);
            }

            updateStatusBar(info, 0);
        }
    }
}

// Контекстное меню для дерева
void MainWindow::on_treeWidget_customContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->treeWidget->itemAt(pos);
    if (!item) {
        return;
    }

    // Создание контекстного меню
    QMenu contextMenu(this);

    // Получение данных элемента
    QVariant data = item->data(0, Qt::UserRole);
    if (!data.isValid() || data.typeId() != QMetaType::QVariantMap) {
        return;
    }

    QVariantMap map = data.toMap();
    QString type = map["type"].toString();

    // Добавление действий в меню
    QAction* extractAction = contextMenu.addAction("Извлечь раздел");
    extractAction->setIcon(QIcon::fromTheme("document-save-as"));

    if (type == "super_container") {
        QAction* analyzeAction = contextMenu.addAction("Анализировать SUPER");
        analyzeAction->setIcon(QIcon::fromTheme("edit-find"));

        contextMenu.addSeparator();

        connect(analyzeAction, &QAction::triggered, this, [this]() {
            on_actionAnalyzeSuper_triggered();
        });
    }

    contextMenu.addSeparator();
    QAction* infoAction = contextMenu.addAction("Информация");
    infoAction->setIcon(QIcon::fromTheme("dialog-information"));

    // Подключение слотов
    connect(extractAction, &QAction::triggered, this, &MainWindow::on_actionExtractPartition_triggered);
    connect(infoAction, &QAction::triggered, this, [this, map]() {
        showPartitionInfo(map);
    });

    // Показ контекстного меню
    contextMenu.exec(ui->treeWidget->viewport()->mapToGlobal(pos));
}

// Двойной клик по элементу таблицы GUID
void MainWindow::on_guidTableWidget_itemDoubleClicked(QTableWidgetItem* item)
{
    if (!item) {
        return;
    }

    int row = item->row();
    QString guid = ui->guidTableWidget->item(row, 0)->text();

    // Отображение детальной информации о GUID
    QString description = m_guidManager->getGuidDescription(guid);
    QString details = QString("Детальная информация о GUID:\n\n"
                              "GUID: %1\n"
                              "Описание: %2\n\n"
                              "Использование: %3")
                          .arg(guid)
                          .arg(description)
                          .arg(getGuidUsage(guid));

    QMessageBox::information(this, "Информация о GUID", details);
}

// Получить информацию об использовании GUID
QString MainWindow::getGuidUsage(const QString& guid) const
{
    // База знаний об использовании GUID в Android
    static const QMap<QString, QString> guidUsage = {
        {"C12A7328-F81F-11D2-BA4B-00A0C93EC93B", "Системный раздел EFI. Используется для загрузки UEFI систем."},
        {"19A710A2-B3CA-11E4-B026-10604B889DCF", "Загрузчик Android. Содержит bootloader и recovery."},
        {"193D1EA4-B3CA-11E4-B075-10604B889DCF", "Раздел boot Android. Ядро и ramdisk."},
        {"A19EA859-4D6F-7442-8235-686F6C746572", "Системный раздел Android. Содержит ОС и системные приложения."},
        {"C5A0AEEC-13EA-11E5-A1B1-001E67CA0C3C", "Vendor раздел. Прошивки и драйверы производителя."},
        {"BD59408B-4514-490D-BF12-9878D963A378", "Пользовательские данные. Приложения, настройки, медиафайлы."},
        {"E6A98E58-E8E4-4C6E-B078-8B3A7A7B5B9E", "Динамический раздел Android. Контейнер для logical partitions."},
        {"0FC63DAF-8483-4772-8E79-3D69D8477DE4", "Файловая система Linux. Стандартный тип для ext2/3/4."},
        {"0657FD6D-A4AB-43C4-84E5-0933C84B4F4F", "Раздел подкачки Linux. Используется для своппинга."}
    };

    // Поиск точного совпадения
    auto it = guidUsage.find(guid.toUpper());
    if (it != guidUsage.end()) {
        return it.value();
    }

    // Поиск частичного совпадения
    for (auto key : guidUsage.keys()) {
        if (guid.contains(key.left(8), Qt::CaseInsensitive)) {
            return guidUsage[key];
        }
    }

    return "Неизвестное использование. Возможно, пользовательский или системный раздел.";
}

// Слот для прогресса файловых операций
void MainWindow::onFileProgressChanged(int current, int total, const QString& message)
{
    Q_UNUSED(total);
    updateProgress(current);
    updateStatusBar(message, 0);
}

