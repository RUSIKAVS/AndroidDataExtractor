#include "MainWindow.hpp"
#include "ui_MainWindow.h"
#include "DeviceInfoParser.hpp"
#include "core/PartitionParser.hpp"
#include "core/KeyParser.hpp"
#include "core/AndroidPartition.hpp"
#include "AndroidCrypto.hpp"
//#include "FileSystemExplorer.hpp"

// Qt Core
#include <QFileDialog>
#include <QMessageBox>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QStatusBar>
#include <QProgressBar>
#include <QSettings>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QDebug>
#include <QComboBox>
#include <QApplication>
#include <QTimer>
#include <QBrush>
#include <QFont>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QProgressDialog>

// Qt Widgets (добавьте эти)
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QRadioButton>
#include <QScrollArea>
#include <QFrame>

// Qt GUI
#include <QColor>

// ==================== КОНСТАНТЫ ====================
const int LOG_DIALOG_WIDTH = 900;
const int LOG_DIALOG_HEIGHT = 700;
const int KEY_DIALOG_WIDTH = 600;
const int KEY_DIALOG_HEIGHT = 400;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , keyParser(new KeyParser())
    , fileExplorer(nullptr)
    , progressBar(new QProgressBar(this))
{
    ui->setupUi(this);

    // Инициализация UI
    initUI();
    initConnections();
    restoreSettings();

    updateStatus("Готов к работе");
}

MainWindow::~MainWindow()
{
    saveSettings();
    cleanupTreeWidget();
    delete keyParser;
    delete ui;
}

// ==================== ИНИЦИАЛИЗАЦИЯ ====================


void MainWindow::initUI()
{
    // Настройка tree widget
    ui->treeWidget->setColumnCount(6);
    ui->treeWidget->setHeaderLabels({
        "Имя раздела", "Тип", "Размер", "Смещение", "Файл", "Статус"
    });
    ui->treeWidget->setSortingEnabled(true);
    ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    // Настройка заголовков
    for (int i = 0; i < 5; ++i) {
        ui->treeWidget->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    ui->treeWidget->header()->setSectionResizeMode(4, QHeaderView::Stretch);

    // Настройка progress bar
    progressBar->setMaximumWidth(200);
    progressBar->setVisible(false);
    //ui->statusbar->addPermanentWidget(progressBar);

    // Настройка текстовых полей
    ui->directoryEdit->setPlaceholderText("Выберите папку с дампом Android...");
    ui->infoText->setReadOnly(true);
    ui->infoText->setFont(QFont("Consolas", 10));
}

void MainWindow::initConnections()
{
    // Кнопки на форме
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::onSelectDirectory);
    connect(ui->logstd, &QPushButton::clicked, this, &MainWindow::showLog);
    connect(ui->analyzeButton, &QPushButton::clicked, this, &MainWindow::onAnalyzeClicked);

    connect(ui->treeWidget, &QTreeWidget::itemClicked,
            this, &MainWindow::onTreeItemClicked);
}

void MainWindow::restoreSettings()
{
    QSettings settings;
    QString lastDir = settings.value("lastDirectory", QDir::homePath()).toString();
    ui->directoryEdit->setText(lastDir);
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("lastDirectory", ui->directoryEdit->text());
}

// ==================== ОСНОВНЫЕ ФУНКЦИИ ====================
void MainWindow::onSelectDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    "Выберите папку с дампом Android",
                                                    ui->directoryEdit->text(),
                                                    QFileDialog::ShowDirsOnly | QFileDialog::ReadOnly);

    if (!dir.isEmpty()) {
        ui->directoryEdit->setText(dir);
        analyzeDirectory(dir);
    }
}

void MainWindow::onAnalyzeClicked()
{
    QString dir = ui->directoryEdit->text();

    if (dir.isEmpty() || !QDir(dir).exists()) {
        QMessageBox::warning(this, "Ошибка",
                             "Пожалуйста, выберите существующую папку.");
        return;
    }

    analyzeDirectory(dir);

    // Показываем информацию о ключах, если они есть
    if (keyParser && !keyParser->getAllKeys().isEmpty()) {
        displayKeyInfo();
    }
}

void MainWindow::analyzeDirectory(const QString& path)
{
    // Сброс предыдущих результатов
    clearDisplay();
    currentPartitions.clear();

    updateStatus("Анализ папки...");
    progressBar->setVisible(true);
    progressBar->setValue(0);
    QApplication::processEvents();

    currentDirectory = path;

    // Загрузка ключей шифрования
    loadEncryptionKeys(path);

    // Сканирование разделов
    updateStatus("Поиск разделов и образов...");

    // Отображаем информацию об устройстве при открытии папки
    displayDeviceInfo(path);

    PartitionParser parser;
    currentPartitions = parser.scanDirectory(path, keyParser);

    progressBar->setValue(50);
    QApplication::processEvents();

    // Отображение результатов
    displayPartitions(currentPartitions);

    progressBar->setValue(100);

    // Статистика
    updateStatistics();

    QTimer::singleShot(1000, this, [this]() {
        progressBar->setVisible(false);
    });
}

void MainWindow::loadEncryptionKeys(const QString& path)
{
    QString keysPath = path + "/keys.json";

    if (QFile::exists(keysPath)) {
        updateStatus("Загрузка ключей шифрования...");

        if (keyParser->parseKeysFile(keysPath)) {
            updateStatus("Ключи шифрования успешно загружены");
            qDebug() << "Загружено ключей:" << keyParser->getAllKeys().size();
        } else {
            updateStatus("Ошибка загрузки keys.json");
            QMessageBox::warning(this, "Ошибка",
                                 "Не удалось прочитать файл keys.json.\n"
                                 "Проверьте формат файла.");
        }
    } else {
        updateStatus("Файл keys.json не найден");
        qDebug() << "keys.json не найден в папке:" << path;
    }
}

void MainWindow::updateStatistics()
{
    int total = currentPartitions.size();
    int encrypted = 0;
    int decryptable = 0;

    for (const auto& part : currentPartitions) {
        if (part.isEncrypted) {
            encrypted++;
            if (part.canBeDecrypted()) {
                decryptable++;
            }
        }
    }

    setWindowTitle(QString("Android Data Extractor - %1 разделов (%2 с ключами) - %3")
                       .arg(total)
                       .arg(decryptable)
                       .arg(QFileInfo(currentDirectory).fileName()));

    updateStatus(QString("Найдено %1 разделов, %2 зашифровано (%3 с ключами)")
                     .arg(total)
                     .arg(encrypted)
                     .arg(decryptable));

    // Подробный вывод в консоль
    qDebug() << "\n=== СТАТИСТИКА АНАЛИЗА ===";
    qDebug() << "Всего разделов:" << total;
    qDebug() << "Зашифровано:" << encrypted;
    qDebug() << "Можно расшифровать:" << decryptable;

    // Поиск важных разделов
    for (const auto& part : currentPartitions) {
        if (part.name.contains("userdata", Qt::CaseInsensitive)) {
            qDebug() << "\nВажный раздел USERDATA:";
            qDebug() << "  Зашифрован:" << (part.isEncrypted ? "Да" : "Нет");
            qDebug() << "  Ключ:" << (part.keyId.isEmpty() ? "Нет" : part.keyId);
            qDebug() << "  Можно расшифровать:" << (part.canBeDecrypted() ? "Да" : "Нет");
        }
    }
}

// ==================== ОТОБРАЖЕНИЕ РАЗДЕЛОВ ====================
void MainWindow::displayPartitions(const QList<AndroidPartition>& partitions)
{
    ui->treeWidget->clear();

    if (partitions.isEmpty()) {
        QTreeWidgetItem* item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, "Разделы не найдены");
        item->setForeground(0, QBrush(Qt::gray));
        return;
    }

    // Группировка по файлам
    QMap<QString, QList<AndroidPartition>> partitionsByFile;
    for (const auto& partition : partitions) {
        partitionsByFile[partition.filePath].append(partition);
    }

    // Создание дерева
    for (auto it = partitionsByFile.begin(); it != partitionsByFile.end(); ++it) {
        QFileInfo fileInfo(it.key());

        QTreeWidgetItem* fileItem = new QTreeWidgetItem(ui->treeWidget);
        fileItem->setText(0, fileInfo.fileName());
        fileItem->setText(1, "Файл образа");
        fileItem->setText(4, it.key());
        fileItem->setData(0, Qt::UserRole, QVariant::fromValue<void*>(nullptr));

        for (const auto& partition : it.value()) {
            addPartitionToTree(partition, fileItem);
        }

        ui->treeWidget->addTopLevelItem(fileItem);
        fileItem->setExpanded(true);
    }

    // Авто-подгонка размеров
    for (int i = 0; i < ui->treeWidget->columnCount(); ++i) {
        ui->treeWidget->resizeColumnToContents(i);
    }
}

void MainWindow::addPartitionToTree(const AndroidPartition& partition, QTreeWidgetItem* parent)
{
    QTreeWidgetItem* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(ui->treeWidget);

    item->setText(0, partition.name);
    item->setText(1, partition.type);
    item->setText(2, partition.humanReadableSize());
    item->setText(3, QString("0x%1").arg(partition.offset, 0, 16));
    item->setText(4, QFileInfo(partition.filePath).fileName());

    // Статус шифрования
    if (partition.isEncrypted) {
        if (partition.canBeDecrypted()) {
            QString status = QString("Зашифрован [%1 через %2]")
                                 .arg(partition.encryptionType)
                                 .arg(partition.keyId);

            if (status.length() > 50) status = status.left(47) + "...";

            item->setText(5, status);
            item->setForeground(5, QBrush(Qt::darkGreen));
            item->setToolTip(5, QString("%1\nКлюч: %2")
                                    .arg(partition.encryptionType)
                                    .arg(partition.keyId));
        } else {
            item->setText(5, "Зашифрован [НЕТ КЛЮЧА]");
            item->setForeground(5, QBrush(Qt::red));
        }
    } else {
        item->setText(5, "Не зашифрован");
        item->setForeground(5, QBrush(Qt::darkGray));
    }

    // Сохраняем копию раздела
    AndroidPartition* partCopy = new AndroidPartition(partition);
    item->setData(0, Qt::UserRole, QVariant::fromValue(partCopy));

    if (!parent) {
        ui->treeWidget->addTopLevelItem(item);
    }
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    QVariant partitionData = item->data(0, Qt::UserRole);
    if (partitionData.isValid()) {
        AndroidPartition* partitionPtr = partitionData.value<AndroidPartition*>();
        if (partitionPtr) {
            showPartitionInfo(*partitionPtr);
        }
    }
}

void MainWindow::showPartitionInfo(const AndroidPartition& partition)
{
    QString info;

    // Заголовок
    info += "📁 Partition Information\n";
    info += "══════════════════════════════════════════════\n\n";

    // Основная информация
    info += QString("• Name:        %1\n").arg(partition.name);
    info += QString("• Type:        %2\n").arg(partition.type);
    info += QString("• Size:        %3\n").arg(partition.humanReadableSize());
    info += QString("• Offset:      0x%4\n").arg(partition.offset, 0, 16);

    // Статус шифрования из первой версии
    info += QString("• Encrypted:   %1\n").arg(partition.isEncrypted ? "YES" : "NO");

    // Тип файла изображения из второй версии
    QString imageType = "Unknown";
    if (partition.filePath.endsWith(".ewc", Qt::CaseInsensitive)) {
        imageType = "EWC Image";
    } else if (partition.filePath.endsWith(".bin", Qt::CaseInsensitive)) {
        imageType = "Raw Binary";
    } else if (partition.filePath.endsWith(".img", Qt::CaseInsensitive)) {
        imageType = "Android Image";
    } else {
        imageType = QFileInfo(partition.filePath).suffix().toUpper() + " Image";
    }
    info += QString("• Image Type:  %1\n").arg(imageType);

    // Имя файла
    info += QString("• File Name:   %1\n").arg(QFileInfo(partition.filePath).fileName());

    // Детальная информация о шифровании, если раздел зашифрован
    if (partition.isEncrypted) {
        info += "\n🔒 Encryption Details:\n";
        info += QString("  - Encryption Type: %1\n")
                    .arg(partition.encryptionType.isEmpty() ? "Unknown" : partition.encryptionType);

        if (!partition.keyId.isEmpty()) {
            info += QString("  - Key ID:         %1\n").arg(partition.keyId);
        }

        // Статус возможности расшифровки
        QString decryptStatus = partition.canBeDecrypted() ? "✅ DECRYPTION POSSIBLE" : "❌ NO KEY AVAILABLE";
        info += QString("  - Status:         %1\n").arg(decryptStatus);

        // Проверяем наличие ключей шифрования
        bool hasKey = false;
        QString foundKeyName;
        QByteArray foundKeyValue;

        // Сначала проверяем наличие ключа по ID
        if (!partition.keyId.isEmpty() && keyParser->hasKey(partition.keyId)) {
            foundKeyValue = keyParser->getKey(partition.keyId);
            if (!foundKeyValue.isEmpty()) {
                hasKey = true;
                foundKeyName = partition.keyId;
                info += QString("  - Found key:      %1\n").arg(partition.keyId);
            }
        }

        // Проверяем стандартные ключи по имени
        if (!hasKey) {
            // Проверяем FDE ключи
            if (partition.keyId.contains("fde", Qt::CaseInsensitive) ||
                partition.encryptionType.contains("fde", Qt::CaseInsensitive)) {

                // Попробуем разные варианты имен FDE ключей
                QStringList fdeKeyNames = {"MTK_FDEKEY", "FDE_KEY", "FDE", "fde_key"};
                for (const QString& keyName : fdeKeyNames) {
                    if (keyParser->hasKey(keyName)) {
                        foundKeyValue = keyParser->getKey(keyName);
                        if (!foundKeyValue.isEmpty()) {
                            hasKey = true;
                            foundKeyName = keyName;
                            info += QString("  - Found key:      %1 (FDE)\n").arg(keyName);
                            break;
                        }
                    }
                }
            }

            // Проверяем RPMB ключи
            if (!hasKey && (partition.keyId.contains("rpmb", Qt::CaseInsensitive) ||
                            partition.encryptionType.contains("rpmb", Qt::CaseInsensitive))) {

                QStringList rpmbKeyNames = {"MTK_RPMBKEY", "RPMB_KEY", "RPMB", "rpmb_key"};
                for (const QString& keyName : rpmbKeyNames) {
                    if (keyParser->hasKey(keyName)) {
                        foundKeyValue = keyParser->getKey(keyName);
                        if (!foundKeyValue.isEmpty()) {
                            hasKey = true;
                            foundKeyName = keyName;
                            info += QString("  - Found key:      %1 (RPMB)\n").arg(keyName);
                            break;
                        }
                    }
                }
            }
        }

        // Пытаемся найти любой ключ по имени раздела
        if (!hasKey) {
            // Создаем список возможных имен ключей на основе имени раздела
            QStringList possibleKeyNames;
            possibleKeyNames << partition.name + "_KEY"
                             << partition.name.toUpper() + "_KEY"
                             << partition.name.toLower() + "_key"
                             << "KEY_" + partition.name.toUpper()
                             << partition.name;

            for (const QString& keyName : possibleKeyNames) {
                if (keyParser->hasKey(keyName)) {
                    foundKeyValue = keyParser->getKey(keyName);
                    if (!foundKeyValue.isEmpty()) {
                        hasKey = true;
                        foundKeyName = keyName;
                        info += QString("  - Found matching key: %1\n").arg(keyName);
                        break;
                    }
                }
            }
        }

        // Если нашли ключ, показываем его
        if (hasKey && !foundKeyValue.isEmpty()) {
            QString keyHex = QString(foundKeyValue.toHex());
            // Обрезаем длинные ключи для отображения
            if (keyHex.length() > 32) {
                keyHex = keyHex.left(16) + "..." + keyHex.right(16);
            }
            info += QString("  - Key value:      %1 (%2 bytes)\n")
                        .arg(keyHex)
                        .arg(foundKeyValue.size());
        } else if (!partition.keyId.isEmpty()) {
            info += "  - ❌ No matching encryption key found\n";
        }

        // Показываем информацию о всех загруженных ключах
        info += "\n  📋 All Loaded Encryption Keys:\n";

        // Получаем список всех ключей (предполагая, что есть метод getKeyList() или getAllKeys())
        // Если такого метода нет, используем hardcoded список известных ключей
        QStringList knownKeyNames = {
            "ChainType", "MTK_CID", "MTK_FDEKEY", "MTK_HRID",
            "MTK_ITRUSTEE", "MTK_ME_ID", "MTK_RID",
            "MTK_RPMB2KEY", "MTK_RPMBKEY", "MTK_SOCID"
        };

        bool anyKeysLoaded = false;
        for (const QString& keyName : knownKeyNames) {
            if (keyParser->hasKey(keyName)) {
                anyKeysLoaded = true;
                QByteArray keyValue = keyParser->getKey(keyName);
                QString keyHex = QString(keyValue.toHex());

                // Обрезаем длинные ключи для отображения
                if (keyHex.length() > 32) {
                    keyHex = keyHex.left(16) + "..." + keyHex.right(16);
                }

                info += QString("    • %1: %2 (%3 bytes)\n")
                            .arg(keyName)
                            .arg(keyHex)
                            .arg(keyValue.size());
            }
        }

        if (!anyKeysLoaded) {
            info += "    ❌ No encryption keys loaded\n";
        }
    }

    // Информация о файле
    info += "\n📊 File Information:\n";
    info += QString("  - Full Path:     %1\n").arg(QDir::toNativeSeparators(partition.filePath));

    QFileInfo fileInfo(partition.filePath);
    info += QString("  - Exists:        %1\n").arg(fileInfo.exists() ? "Yes" : "No");

    if (fileInfo.exists()) {
        info += QString("  - Size:          %1 bytes\n").arg(fileInfo.size());
        info += QString("  - Last Modified: %1\n").arg(fileInfo.lastModified().toString("yyyy-MM-dd HH:mm:ss"));

        if (fileInfo.birthTime().isValid()) {
            info += QString("  - Created:       %1\n").arg(fileInfo.birthTime().toString("yyyy-MM-dd HH:mm:ss"));
        }

        info += QString("  - Readable:      %1\n").arg(fileInfo.isReadable() ? "Yes" : "No");
        info += QString("  - Writable:      %1\n").arg(fileInfo.isWritable() ? "Yes" : "No");
    }


    // EWC информация, если есть
    if (keyParser->hasEwcInfo()) {
        info += "\n🔐 EWC Project Information:\n";
        info += "  - This is an EWC project file\n";

        // Если в вашем KeyParser есть метод для получения EWC информации, используйте его
        // Например: keyParser->getEwcProjectName() или keyParser->getEwcVersion()
        // Если нет - просто показываем общую информацию
        info += "  - Contains embedded checksums\n";
        info += "  - May include multiple partition images\n";
    }


    // Hex dump заголовка файла (если доступен)
    QFile file(partition.filePath);
    if (file.open(QIODevice::ReadOnly)) {
        qint64 readOffset = partition.offset;
        if (readOffset >= 0 && file.seek(readOffset)) {
            QByteArray header = file.read(512);
            file.close();

            if (!header.isEmpty()) {
                info += "\n📄 Raw Data Header (first 512 bytes):\n";
                info += QString("  - Hex dump (first 64 bytes from offset 0x%1):\n").arg(readOffset, 0, 16);

                for (int i = 0; i < qMin(64, header.size()); i += 16) {
                    QString hexLine;
                    QString asciiLine;

                    for (int j = 0; j < 16 && (i + j) < header.size(); j++) {
                        unsigned char c = (unsigned char)header[i + j];
                        hexLine += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();

                        if (c >= 32 && c <= 126) {
                            asciiLine += QChar(c);
                        } else {
                            asciiLine += '.';
                        }
                    }

                    // Выравнивание hex строки
                    while (hexLine.length() < 48) {
                        hexLine += ' ';
                    }

                    info += QString("    %1 | %2\n").arg(hexLine).arg(asciiLine);
                }

                // Анализ магических чисел
                if (header.size() >= 4) {
                    QString magic = QString(header.left(4).toHex()).toUpper();
                    info += QString("  - Magic bytes:    0x%1\n").arg(magic);

                    // Определение типа файла по магическим числам
                    if (magic == "53454D43") { // SEMC
                        info += "  - Detected:       Sony ELF format\n";
                    } else if (magic == "7F454C46") { // .ELF
                        info += "  - Detected:       ELF executable\n";
                    } else if (header.startsWith("ANDROID!")) {
                        info += "  - Detected:       Android boot image\n";
                    } else if (header.startsWith("CHROMEOS")) {
                        info += "  - Detected:       Chrome OS image\n";
                    }
                }
            }
        }
    }

    // Итоговый статус
    info += "\n══════════════════════════════════════════════\n";
    if (partition.isEncrypted) {
        // Проверяем еще раз наличие ключа для окончательного статуса
        bool canDecrypt = partition.canBeDecrypted();

        // Дополнительная проверка через keyParser
        if (!canDecrypt && !partition.keyId.isEmpty()) {
            canDecrypt = keyParser->hasKey(partition.keyId);
        }

        if (canDecrypt) {
            info += "✅ READY - This partition can be decrypted\n";
        } else {
            info += "⚠️  WARNING - No decryption key available\n";
        }
    } else {
        info += "✅ READY - This partition can be processed\n";
    }

    ui->infoText->setPlainText(info);
}

// ==================== ИЗВЛЕЧЕНИЕ РАЗДЕЛОВ ====================
void MainWindow::onExtractPartition()
{
    AndroidPartition* partition = getSelectedPartition();
    if (!partition) {
        QMessageBox::information(this, "Не выбрано",
                                 "Пожалуйста, выберите раздел для извлечения.");
        return;
    }

    // Запрос места сохранения
    QString defaultName = QString("%1_%2.bin")
                              .arg(partition->name)
                              .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    QString savePath = QFileDialog::getSaveFileName(this,
                                                    "Сохранить раздел",
                                                    QDir::homePath() + "/" + defaultName,
                                                    "Бинарные файлы (*.bin);;Все файлы (*.*)");

    if (savePath.isEmpty()) return;

    // Получение ключа дешифрования
    QByteArray decryptionKey;
    if (!partition->keyId.isEmpty() && keyParser->hasKey(partition->keyId)) {
        decryptionKey = keyParser->getKey(partition->keyId);
        updateStatus(QString("Используется ключ '%1' для расшифровки").arg(partition->keyId));
    }

    // Извлечение
    updateStatus(QString("Извлечение %1...").arg(partition->name));
    progressBar->setVisible(true);
    progressBar->setRange(0, 0);

    PartitionParser parser;
    bool success = parser.extractPartition(*partition, savePath, decryptionKey);

    progressBar->setVisible(false);

    // Обработка результата
    if (success) {
        updateStatus(QString("Успешно извлечено в %1").arg(savePath));
        QMessageBox::information(this, "Успех",
                                 QString("Раздел '%1' успешно извлечен в:\n%2")
                                     .arg(partition->name)
                                     .arg(savePath));
    } else {
        updateStatus("Ошибка извлечения");
        QMessageBox::critical(this, "Ошибка",
                              QString("Не удалось извлечь раздел '%1'").arg(partition->name));
    }
}

void MainWindow::onExtractAllPartitions()
{
    if (currentPartitions.isEmpty()) {
        QMessageBox::information(this, "Нет разделов",
                                 "Нет разделов для извлечения. Сначала проанализируйте папку.");
        return;
    }

    QString dir = QFileDialog::getExistingDirectory(this,
                                                    "Выберите папку для сохранения",
                                                    QDir::homePath(),
                                                    QFileDialog::ShowDirsOnly);

    if (dir.isEmpty()) return;

    QDir outputDir(dir);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString extractDir = QString("android_extract_%1").arg(timestamp);

    if (!outputDir.mkdir(extractDir)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать папку.");
        return;
    }

    outputDir.cd(extractDir);

    updateStatus("Извлечение всех разделов...");
    progressBar->setVisible(true);
    progressBar->setMaximum(currentPartitions.size());

    int successCount = 0;
    PartitionParser parser;

    for (int i = 0; i < currentPartitions.size(); ++i) {
        const AndroidPartition& partition = currentPartitions[i];

        QString outputPath = outputDir.absoluteFilePath(
            QString("%1_%2.bin")
                .arg(partition.name)
                .arg(QDateTime::currentDateTime().toString("hhmmss")));

        updateStatus(QString("Извлечение %1 (%2/%3)...")
                         .arg(partition.name)
                         .arg(i + 1)
                         .arg(currentPartitions.size()));

        progressBar->setValue(i + 1);
        QApplication::processEvents();

        // Ключ для дешифрования
        QByteArray decryptionKey;
        if (!partition.keyId.isEmpty() && keyParser->hasKey(partition.keyId)) {
            decryptionKey = keyParser->getKey(partition.keyId);
        }

        if (parser.extractPartition(partition, outputPath, decryptionKey)) {
            successCount++;
        }
    }

    progressBar->setVisible(false);

    QMessageBox::information(this, "Завершено",
                             QString("Извлечение завершено.\n"
                                     "Успешно: %1 из %2\n\n"
                                     "Папка: %3")
                                 .arg(successCount)
                                 .arg(currentPartitions.size())
                                 .arg(outputDir.absolutePath()));

    updateStatus(QString("Извлечено %1 из %2 разделов")
                     .arg(successCount)
                     .arg(currentPartitions.size()));
}

// ==================== РАБОТА С КЛЮЧАМИ ====================
void MainWindow::displayKeyInfo()
{
    if (!keyParser) return;

    QDialog keyDialog(this);
    keyDialog.setWindowTitle("Загруженные ключи шифрования");
    keyDialog.resize(KEY_DIALOG_WIDTH, KEY_DIALOG_HEIGHT);

    QVBoxLayout* layout = new QVBoxLayout(&keyDialog);

    // Таблица ключей
    QTableWidget* table = new QTableWidget(&keyDialog);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({"Имя ключа", "Значение (первые 32 символа)", "Описание"});
    table->horizontalHeader()->setStretchLastSection(true);

    auto allKeys = keyParser->getAllKeys();
    table->setRowCount(allKeys.size());

    int row = 0;
    for (auto it = allKeys.begin(); it != allKeys.end(); ++it) {
        QString keyName = it.key();
        QString keyValue = keyParser->getKey(keyName).toHex();
        QString description = it.value();

        QString displayValue = keyValue.length() > 32 ?
                                   keyValue.left(32) + "..." : keyValue;

        table->setItem(row, 0, new QTableWidgetItem(keyName));
        table->setItem(row, 1, new QTableWidgetItem(displayValue));
        table->setItem(row, 2, new QTableWidgetItem(description));

        // Выделение важных ключей
        if (keyName == "MTK_RPMBKEY" || keyName == "MTK_FDEKEY") {
            for (int col = 0; col < 3; ++col) {
                table->item(row, col)->setBackground(QBrush(QColor(255, 255, 200)));
            }
        }

        row++;
    }

    table->resizeColumnsToContents();

    // Сводка
    QLabel* summary = new QLabel(&keyDialog);
    summary->setText(QString(
                         "Всего ключей: %1\n"
                         "RPMB Key: %2\n"
                         "FDE Key: %3\n"
                         "ME ID: %4")
                         .arg(allKeys.size())
                         .arg(keyParser->getRpmbKey().isEmpty() ? "Не найден" : "Доступен")
                         .arg(keyParser->getFdeKey().isEmpty() ? "Не найден" : "Доступен")
                         .arg(keyParser->getMeId().isEmpty() ? "Не найден" : "Доступен"));

    QPushButton* closeButton = new QPushButton("Закрыть", &keyDialog);
    connect(closeButton, &QPushButton::clicked, &keyDialog, &QDialog::accept);

    layout->addWidget(summary);
    layout->addWidget(table);
    layout->addWidget(closeButton);

    keyDialog.exec();
}

// ==================== ЛОГ И ОТЧЕТЫ ====================
void MainWindow::showLog()
{
    QDialog logDialog(this);
    logDialog.setWindowTitle("Лог анализа - Android Data Extractor");
    logDialog.resize(LOG_DIALOG_WIDTH, LOG_DIALOG_HEIGHT);

    QVBoxLayout* layout = new QVBoxLayout(&logDialog);


    QTextEdit* logText = new QTextEdit(&logDialog);
    logText->setReadOnly(true);
    logText->setFont(QFont("Consolas", 10));

    // Генерация отчета
    QString report = generateAnalysisReport();
    logText->setPlainText(report);

    // Кнопки
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    QPushButton* saveButton = new QPushButton("💾 Сохранить отчет...", &logDialog);
    QPushButton* extractButton = new QPushButton("🔓 Извлечь Userdata", &logDialog);
    QPushButton* closeButton = new QPushButton("✕ Закрыть", &logDialog);

    saveButton->setStyleSheet("QPushButton { padding: 5px 15px; font-weight: bold; }");
    extractButton->setStyleSheet(
        "QPushButton { padding: 5px 15px; font-weight: bold; "
        "background-color: #4CAF50; color: white; }");

    connect(saveButton, &QPushButton::clicked, [&]() { saveReport(report); });
    connect(extractButton, &QPushButton::clicked, &logDialog, &QDialog::accept);
    connect(closeButton, &QPushButton::clicked, &logDialog, &QDialog::accept);

    connect(extractButton, &QPushButton::clicked, [&]() {
        extractUserdataPartition();
    });

    buttonLayout->addWidget(saveButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(extractButton);
    buttonLayout->addWidget(closeButton);

    layout->addWidget(logText);
    layout->addLayout(buttonLayout);

    logDialog.exec();
}

QString MainWindow::generateAnalysisReport() const
{
    QString report;

    // Заголовок
    report += QString(80, '=') + "\n";
    report += "ANDROID DATA EXTRACTOR - ОТЧЕТ АНАЛИЗА\n";
    report += QString(80, '=') + "\n";
    report += "Время: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") + "\n";
    report += "Папка: " + currentDirectory + "\n\n";

    // Сводка
    report += "СВОДКА\n";
    report += QString(40, '-') + "\n";

    int total = currentPartitions.size();
    int encrypted = 0;
    int decryptable = 0;

    for (const auto& part : currentPartitions) {
        if (part.isEncrypted) {
            encrypted++;
            if (part.canBeDecrypted()) decryptable++;
        }
    }

    report += QString("Всего разделов: %1\n").arg(total);
    report += QString("Зашифровано: %1\n").arg(encrypted);
    report += QString("Можно расшифровать: %1\n\n").arg(decryptable);

    // Ключи
    if (keyParser) {
        auto allKeys = keyParser->getAllKeys();
        report += "КЛЮЧИ ШИФРОВАНИЯ\n";
        report += QString(40, '-') + "\n";
        report += QString("Загружено ключей: %1\n").arg(allKeys.size());
        report += QString("RPMB Key: %1\n").arg(
            keyParser->getRpmbKey().isEmpty() ? "НЕТ ✗" : "ЕСТЬ ✓");
        report += QString("FDE Key: %1\n\n").arg(
            keyParser->getFdeKey().isEmpty() ? "НЕТ ✗" : "ЕСТЬ ✓");
    }

    // Важные разделы
    report += "ВАЖНЫЕ РАЗДЕЛЫ\n";
    report += QString(40, '-') + "\n";

    for (const auto& part : currentPartitions) {
        QString lowerName = part.name.toLower();
        if (lowerName.contains("userdata") || lowerName.contains("data") ||
            part.name == "super" || part.name == "metadata") {

            QString status = part.isEncrypted ?
                                 (part.canBeDecrypted() ? "✓" : "✗") : "○";

            report += QString("%1 %2\n").arg(status).arg(part.name);
            report += QString("  Размер: %1\n").arg(part.humanReadableSize());
            report += QString("  Зашифрован: %1\n").arg(part.isEncrypted ? "Да" : "Нет");

            if (part.isEncrypted) {
                report += QString("  Ключ: %1\n").arg(
                    part.keyId.isEmpty() ? "Нет" : part.keyId);
                report += QString("  Статус: %1\n").arg(
                    part.canBeDecrypted() ? "Можно расшифровать" : "Нет ключа");
            }
            report += "\n";
        }
    }

    // Все разделы
    report += QString("ВСЕ РАЗДЕЛЫ (%1)\n").arg(total);
    report += QString(80, '=') + "\n\n";

    for (int i = 0; i < currentPartitions.size(); ++i) {
        const auto& part = currentPartitions[i];

        QString prefix = "○ ";
        if (part.isEncrypted && part.canBeDecrypted()) prefix = "✓ ";
        else if (part.isEncrypted && !part.canBeDecrypted()) prefix = "✗ ";

        report += QString("[%1] %2%3\n")
                      .arg(i + 1, 2)
                      .arg(prefix)
                      .arg(part.name);
    }

    // Заключение
    report += QString(80, '=') + "\n";
    report += "ЗАКЛЮЧЕНИЕ\n";
    report += QString(40, '-') + "\n";

    bool userdataFound = false;
    for (const auto& part : currentPartitions) {
        if (part.name.toLower().contains("userdata")) {
            userdataFound = true;

            if (part.isEncrypted && part.canBeDecrypted()) {
                report += "✓ USERDATA МОЖНО РАСШИФРОВАТЬ!\n";
                report += "  Раздел userdata зашифрован и есть ключ для расшифровки.\n";
                report += "  Используйте функцию 'Извлечь' для расшифровки данных.\n";
            } else if (part.isEncrypted && !part.canBeDecrypted()) {
                report += "✗ USERDATA ЗАШИФРОВАН, НО КЛЮЧА НЕТ\n";
                report += "  Для расшифровки нужен ключ MTK_RPMBKEY.\n";
            } else {
                report += "○ USERDATA НЕ ЗАШИФРОВАН\n";
                report += "  Данные можно извлечь без расшифровки.\n";
            }
            break;
        }
    }

    if (!userdataFound) {
        report += "⚠ USERDATA НЕ НАЙДЕН\n";
        report += "  Раздел userdata не обнаружен в дампе.\n";
    }

    report += QString(80, '=') + "\n";

    return report;
}

void MainWindow::saveReport(const QString& report)
{
    QString fileName = QFileDialog::getSaveFileName(
        nullptr,
        "Сохранить отчет анализа",
        QDir::homePath() + "/android_analysis_report.txt",
        "Текстовые файлы (*.txt);;Все файлы (*.*)");

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << report;
            file.close();
            QMessageBox::information(nullptr, "Сохранено", "Отчет успешно сохранен.");
        }
    }
}

void MainWindow::extractUserdataPartition()
{
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = ui->treeWidget->topLevelItem(i);
        AndroidPartition* part = getPartitionFromItem(item);

        if (part && part->name.toLower().contains("userdata")) {
            ui->treeWidget->setCurrentItem(item);
            onExtractPartition();
            return;
        }
    }

    QMessageBox::information(this, "Не найден",
                             "Раздел userdata не найден в списке.");
}

// ==================== ТЕСТИРОВАНИЕ И ПОДБОР ПАРОЛЕЙ ====================
void MainWindow::testWithKnownPassword()
{
    AndroidPartition* partition = getSelectedPartition();
    if (!partition) {
        QMessageBox::information(this, "Не выбрано",
                                 "Выберите зашифрованный раздел.");
        return;
    }

    if (!partition->isEncrypted) {
        QMessageBox::information(this, "Не зашифрован",
                                 "Выбранный раздел не зашифрован.");
        return;
    }

    // Создание диалога
    QDialog dialog(this);
    dialog.setWindowTitle("Тест пароля - " + partition->name);
    dialog.resize(500, 400);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // Информация
    QLabel* infoLabel = new QLabel(
        QString("<b>Тестирование пароля для раздела:</b> %1<br>"
                "<b>Шифрование:</b> %2<br>"
                "<b>Размер:</b> %3")
            .arg(partition->name)
            .arg(partition->encryptionType)
            .arg(partition->humanReadableSize()));
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // Поле для пароля
    QLineEdit* passwordEdit = new QLineEdit();
    passwordEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(new QLabel("Пароль для теста:"));
    layout->addWidget(passwordEdit);

    // Показать/скрыть пароль
    QCheckBox* showPassCheck = new QCheckBox("Показать пароль");
    connect(showPassCheck, &QCheckBox::toggled, [passwordEdit](bool checked) {
        passwordEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    layout->addWidget(showPassCheck);

    // Тестовые пароли
    QGroupBox* testGroup = new QGroupBox("Быстрые тесты:");
    QHBoxLayout* testLayout = new QHBoxLayout(testGroup);

    QStringList testPasswords = {"1234", "0000", "1111", "password", "admin"};
    for (const QString& pass : testPasswords) {
        QPushButton* btn = new QPushButton(pass);
        btn->setMaximumWidth(70);
        connect(btn, &QPushButton::clicked, [passwordEdit, pass]() {
            passwordEdit->setText(pass);
        });
        testLayout->addWidget(btn);
    }
    layout->addWidget(testGroup);

    // Результат
    QTextEdit* resultText = new QTextEdit();
    resultText->setReadOnly(true);
    resultText->setMaximumHeight(100);
    layout->addWidget(new QLabel("Результат:"));
    layout->addWidget(resultText);

    // Кнопки
    QPushButton* testBtn = new QPushButton("🔍 Тестировать");
    QPushButton* cancelBtn = new QPushButton("Отмена");

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addWidget(testBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    // Тестирование
    connect(testBtn, &QPushButton::clicked, [&]() {
        QString password = passwordEdit->text().trimmed();

        if (password.isEmpty()) {
            resultText->setHtml("<font color='red'>Введите пароль для теста.</font>");
            return;
        }

        // Чтение тестовых данных
        QFile file(partition->filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            resultText->setHtml("<font color='red'>Не удалось открыть файл раздела.</font>");
            return;
        }

        file.seek(partition->offset);
        QByteArray testData = file.read(4096);
        file.close();

        if (testData.isEmpty()) {
            resultText->setHtml("<font color='red'>Не удалось прочитать данные раздела.</font>");
            return;
        }

        // Тестирование
        resultText->setHtml("<i>Тестирование пароля...</i>");
        QApplication::processEvents();

        QByteArray salt = partition->name.toUtf8();
        bool success = AndroidCrypto::tryDecryptWithPassword(
            testData, password, salt, AndroidCrypto::CRYPTO_MTK_AES_CBC);

        // Результат
        if (success) {
            resultText->setHtml(QString(
                                    "<font color='green'><b>✅ ПАРОЛЬ ПОДОЙДЕТ!</b></font><br>"
                                    "<b>Пароль:</b> %1<br>"
                                    "<b>Раздел:</b> %2<br>"
                                    "Теперь можно использовать этот пароль для извлечения данных.")
                                    .arg(password)
                                    .arg(partition->name));
        } else {
            resultText->setHtml(QString(
                                    "<font color='red'><b>❌ ПАРОЛЬ НЕ ПОДОЙДЕТ</b></font><br>"
                                    "<b>Пароль:</b> %1<br>"
                                    "Этот пароль не расшифровывает данные.")
                                    .arg(password));
        }
    });

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    dialog.exec();
}

void MainWindow::testDecryption()
{
    testWithKnownPassword(); // Используем существующий метод
}


// ==================== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ====================
AndroidPartition* MainWindow::getSelectedPartition()
{
    QTreeWidgetItem* item = ui->treeWidget->currentItem();
    return getPartitionFromItem(item);
}

AndroidPartition* MainWindow::getPartitionFromItem(QTreeWidgetItem* item)
{
    if (!item) return nullptr;

    QVariant partitionData = item->data(0, Qt::UserRole);
    if (!partitionData.isValid()) return nullptr;

    return partitionData.value<AndroidPartition*>();
}

void MainWindow::clearDisplay()
{
    ui->treeWidget->clear();
    ui->infoText->clear();
}

void MainWindow::updateStatus(const QString& message)
{
    //ui->statusbar->showMessage(message, 5000);
}

void MainWindow::updateProgress(int value)
{
    progressBar->setValue(value);
}

void MainWindow::cleanupTreeWidget()
{
    QTreeWidgetItemIterator it(ui->treeWidget);
    while (*it) {
        QVariant data = (*it)->data(0, Qt::UserRole);
        if (data.isValid()) {
            AndroidPartition* partition = data.value<AndroidPartition*>();
            delete partition;
        }
        ++it;
    }
}

// ==================== ПРОСТЫЕ СЛОТЫ ====================
void MainWindow::onAbout()
{
    QMessageBox::about(this, "О программе",
                       "<h3>Android Data Extractor</h3>"
                       "<p>Версия 1.0.0</p>"
                       "<p>Программа для извлечения и анализа данных из дампов Android устройств.</p>"
                       "<p>Поддерживает:</p>"
                       "<ul>"
                       "<li>Таблицы разделов GPT и MBR</li>"
                       "<li>Android sparse образы</li>"
                       "<li>Зашифрованные разделы (через keys.json)</li>"
                       "<li>Сырые файлы разделов</li>"
                       "<li>Подбор паролей для зашифрованных разделов</li>"
                       "</ul>"
                       "<p>Разработано на Qt 6</p>");
}

void MainWindow::onGenerateReport()
{
    showLog(); // Используем существующий метод
}

void MainWindow::onQuickExtract()
{
    onExtractPartition(); // Простое извлечение
}

void MainWindow::onViewKeys()
{
    displayKeyInfo();
}

void MainWindow::onRefresh()
{
    if (!currentDirectory.isEmpty()) {
        analyzeDirectory(currentDirectory);
    }
}

void MainWindow::onToggleDarkMode()
{
    static bool darkMode = false;
    darkMode = !darkMode;

    if (darkMode) {
        qApp->setStyleSheet(
            "QMainWindow { background-color: #2b2b2b; color: #ffffff; }"
            "QWidget { background-color: #2b2b2b; color: #ffffff; }"
            "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }"
            "QTreeWidget { background-color: #1e1e1e; color: #d4d4d4; }"
            "QLineEdit { background-color: #3c3c3c; color: #ffffff; }"
            "QPushButton { background-color: #3c3c3c; color: #ffffff; }"
            "QGroupBox { color: #ffffff; }"
            );
    } else {
        qApp->setStyleSheet("");
    }
}

void MainWindow::onCheckUpdates()
{
    QMessageBox::information(this, "Обновления",
                             "Проверка обновлений еще не реализована.\n"
                             "Текущая версия: 1.0.0");
}

void MainWindow::onDocumentation()
{
    QMessageBox::information(this, "Документация",
                             "Android Data Extractor - MTK Specialist\n\n"
                             "Как использовать:\n"
                             "1. Выберите папку с дампом Android\n"
                             "2. Нажмите 'Анализ' для сканирования разделов\n"
                             "3. Просмотрите разделы в списке\n"
                             "4. Используйте инструменты для извлечения или расшифровки\n\n"
                             "Для устройств MTK убедитесь, что у вас есть:\n"
                             "- Файл keys.json с ключами RPMB/FDE\n"
                             "- Полный дамп eMMC накопителя");
}

void MainWindow::createTestScenario()
{
    // Создание тестового файла
    QString testFile = QCoreApplication::applicationDirPath() + "/test_encrypted.bin";

    QByteArray originalData;
    for (int i = 0; i < 65536; i++) {
        originalData.append(static_cast<char>(i % 256));
    }

    QString testPassword = "9999";
    QByteArray salt = "test";

    QByteArray key = AndroidCrypto::deriveKeyPbkdf2(testPassword, salt, 10000, 32);
    AndroidCrypto::CryptoParams params;
    params.key = key.left(32);
    params.iv = key.mid(32, 16);
    params.type = AndroidCrypto::CRYPTO_MTK_AES_CBC;

    QByteArray encrypted = AndroidCrypto::decryptData(originalData, params, 0);

    QFile file(testFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(encrypted);
        file.close();
        qDebug() << "Тестовый файл создан:" << testFile;
    }

    // Тестирование
    QByteArray testEncrypted = encrypted;
    bool success1 = AndroidCrypto::tryDecryptWithPassword(
        testEncrypted, "9999", salt, AndroidCrypto::CRYPTO_MTK_AES_CBC);
    bool success2 = AndroidCrypto::tryDecryptWithPassword(
        testEncrypted, "0000", salt, AndroidCrypto::CRYPTO_MTK_AES_CBC);

    qDebug() << "Тест с правильным паролем (9999):" << (success1 ? "УСПЕХ" : "ОШИБКА");
    qDebug() << "Тест с неправильным паролем (0000):" << (success2 ? "ЛОЖНОЕ СРАБАТЫВАНИЕ" : "КОРРЕКТНО");
}

void MainWindow::onShowMemoryMap()
{
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "Выберите файл дампа",
                                                    ui->directoryEdit->text(),
                                                    "Файлы образов (*.img *.bin *.raw);;Все файлы (*.*)");

    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть файл.");
        return;
    }

    qint64 fileSize = file.size();
    QByteArray header = file.read(4096);
    file.close();

    QString result = "Анализ карты памяти\n";
    result += "=====================\n";
    result += QString("Файл: %1\n").arg(QFileInfo(filePath).fileName());
    result += QString("Размер: %1 байт (%2 ГБ)\n\n").arg(fileSize)
                  .arg(fileSize / (1024.0 * 1024 * 1024), 0, 'f', 2);

    result += "Найдены сигнатуры:\n";
    if (header.size() >= 8) {
        if (header.startsWith("ANDROID!")) result += "- Android boot image\n";
        if (header.contains("EFI PART")) result += "- GPT таблица разделов\n";
        if (header.size() >= 512 && header.at(510) == 0x55 && header.at(511) == 0xAA)
            result += "- MBR таблица разделов\n";
        if (header.contains("LUKS")) result += "- LUKS шифрование\n";
        if (header.contains("MTK")) result += "- MediaTek формат\n";
    }

    // Показать результат
    QDialog dialog(this);
    dialog.setWindowTitle("Результаты анализа");
    dialog.resize(600, 400);

    QTextEdit* textEdit = new QTextEdit();
    textEdit->setPlainText(result);
    textEdit->setReadOnly(true);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->addWidget(textEdit);

    QPushButton* closeButton = new QPushButton("Закрыть");
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeButton);

    dialog.exec();
}

void MainWindow::onForceRawParse()
{
    QString filePath;

    // Поиск файлов в текущей директории
    if (!ui->directoryEdit->text().isEmpty()) {
        QDir dir(ui->directoryEdit->text());
        QStringList files = dir.entryList({"*.img", "*.bin", "*.raw"},
                                          QDir::Files | QDir::Readable);

        if (!files.isEmpty()) {
            bool ok;
            QString selected = QInputDialog::getItem(this,
                                                     "Выбор файла",
                                                     "Выберите файл для анализа:",
                                                     files, 0, false, &ok);

            if (ok && !selected.isEmpty()) {
                filePath = dir.absoluteFilePath(selected);
            }
        }
    }

    // Стандартный диалог
    if (filePath.isEmpty()) {
        filePath = QFileDialog::getOpenFileName(this,
                                                "Выберите файл дампа",
                                                ui->directoryEdit->text().isEmpty() ? QDir::homePath() : ui->directoryEdit->text(),
                                                "Файлы образов (*.img *.bin *.raw);;Все файлы (*.*)");
    }

    if (filePath.isEmpty()) return;

    updateStatus("Анализ сырого дампа...");

    // Прогресс
    QProgressDialog progress("Анализ сырого дампа...", "Отмена", 0, 100, this);
    progress.setWindowTitle("Анализ");
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();

    // Парсинг в отдельном потоке
    QFuture<QList<AndroidPartition>> future = QtConcurrent::run([=]() {
        PartitionParser parser;
        QList<AndroidPartition> partitions = parser.parseRawDump(filePath);

        if (keyParser && !partitions.isEmpty()) {
            for (auto& part : partitions) {
                part = parser.analyzePartitionWithKeys(part, keyParser);
            }
        }

        return partitions;
    });

    // Ожидание завершения
    while (!future.isFinished()) {
        QApplication::processEvents();
        progress.setValue((progress.value() + 5) % 100);
        QThread::msleep(50);
    }

    progress.close();

    QList<AndroidPartition> partitions = future.result();

    if (partitions.isEmpty()) {
        QMessageBox::warning(this, "Не найдено",
                             "Не удалось найти разделы в дампе.\n"
                             "Попробуйте другой файл или используйте 'Показать карту памяти'.");
        return;
    }

    // Обновление интерфейса
    currentPartitions = partitions;
    displayPartitions(partitions);

    updateStatus(QString("Найдено %1 разделов в сыром дампе").arg(partitions.size()));

    // Показать отчет
    QString result = QString(
                         "АНАЛИЗ СЫРОГО ДАМПА\n"
                         "==================\n"
                         "Файл: %1\n"
                         "Размер: %2 ГБ\n"
                         "Разделов найдено: %3\n\n")
                         .arg(QFileInfo(filePath).fileName())
                         .arg(QFileInfo(filePath).size() / (1024.0*1024*1024), 0, 'f', 2)
                         .arg(partitions.size());

    for (int i = 0; i < partitions.size(); ++i) {
        const auto& part = partitions[i];
        result += QString("%1. %2\n").arg(i+1).arg(part.name);
        result += QString("   Смещение: 0x%1\n").arg(part.offset, 0, 16);
        result += QString("   Размер: %1\n").arg(part.humanReadableSize());
        result += QString("   Тип: %1\n").arg(part.type);
        result += QString("   Зашифрован: %1\n").arg(part.isEncrypted ? "Да" : "Нет");
        if (part.isEncrypted) {
            result += QString("   Ключ: %1\n").arg(part.keyId.isEmpty() ? "Нет" : part.keyId);
        }
        result += "\n";
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Результаты анализа дампа");
    dialog.resize(600, 500);

    QTextEdit* textEdit = new QTextEdit();
    textEdit->setPlainText(result);
    textEdit->setReadOnly(true);
    textEdit->setFont(QFont("Consolas", 10));

    QPushButton* closeButton = new QPushButton("Закрыть");
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->addWidget(textEdit);
    layout->addWidget(closeButton);

    dialog.exec();
}

void MainWindow::showFileSystemExplorer()
{
    AndroidPartition* partition = getSelectedPartition();
    if (!partition) {
        QMessageBox::information(this, "Не выбрано", "Выберите раздел.");
        return;
    }

    if (!partition->canBeDecrypted()) {
        QMessageBox::warning(this, "Нельзя расшифровать",
                             "Нет ключа для расшифровки этого раздела.");
        return;
    }

    if (!FileSystemExplorer::supportsFilesystem(partition->type)) {
        QMessageBox::warning(this, "Не поддерживается",
                             QString("Файловая система '%1' не поддерживается.\n"
                                     "Поддерживаются: ext2/3/4, f2fs, erofs, fat.")
                                 .arg(partition->type));
        return;
    }

    if (!fileExplorer) {
        fileExplorer = new FileSystemExplorer();
        fileExplorer->setWindowTitle("Просмотр файловой системы");
        fileExplorer->resize(1000, 700);
    }

    // Получение ключа
    QByteArray decryptionKey;
    if (!partition->keyId.isEmpty()) {
        decryptionKey = keyParser->getKey(partition->keyId);
    }

    // Монтирование
    QProgressDialog progress("Монтирование раздела...", "Отмена", 0, 0, this);
    progress.setWindowTitle("Монтирование");
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    QApplication::processEvents();

    bool mounted = fileExplorer->mountPartition(*partition, decryptionKey);

    progress.close();

    if (mounted) {
        fileExplorer->show();
        fileExplorer->raise();
        fileExplorer->activateWindow();
    } else {
        QMessageBox::critical(this, "Ошибка",
                              "Не удалось смонтировать раздел.\n"
                              "Возможные причины:\n"
                              "1. Неправильный ключ дешифрования\n"
                              "2. Неподдерживаемая файловая система\n"
                              "3. Отсутствие системных утилит (Linux/Mac)\n"
                              "4. Отсутствие драйверов (Windows)");
    }
}

// Новый метод для отображения информации об устройстве
void MainWindow::displayDeviceInfo(const QString& path)
{
    QString infoText;
    QTextStream stream(&infoText);

    stream << "Dump Device Information\n";
    stream << "══════════════════════════════════════════════\n\n";

    // Получаем информацию об устройстве
    DeviceInfo deviceInfo = DeviceInfoParser::parseFromDirectory(path);

    // Выводим основную информацию
    if (!deviceInfo.brand.isEmpty() && deviceInfo.brand != "Unknown") {
        stream << "• Brand:        " << deviceInfo.brand << "\n";
    }

    if (!deviceInfo.model.isEmpty() && deviceInfo.model != "Unknown") {
        stream << "• Model:        " << deviceInfo.model << "\n";
    }

    if (!deviceInfo.androidVersion.isEmpty() && deviceInfo.androidVersion != "Unknown") {
        stream << "• Android ver:  " << deviceInfo.androidVersion << "\n";
    }

    if (!deviceInfo.firmwareDate.isEmpty() && deviceInfo.firmwareDate != "Unknown") {
        stream << "• Firmware data: " << deviceInfo.firmwareDate << "\n";
    }

    // ВАЖНО: используем securityPatchDate, а не secPatchDate
    if (!deviceInfo.securityPatchDate.isEmpty() && deviceInfo.securityPatchDate != "Unknown") {
        stream << "• SecPatch data: " << deviceInfo.securityPatchDate << "\n";
    }

    if (!deviceInfo.platform.isEmpty() && deviceInfo.platform != "Unknown") {
        stream << "• Platform:      " << deviceInfo.platform << "\n";
    }

    if (!deviceInfo.imei1.isEmpty() && deviceInfo.imei1 != "Unknown") {
        stream << "• IMEI 1:       " << deviceInfo.imei1 << "\n";
    }

    if (!deviceInfo.imei2.isEmpty() && deviceInfo.imei2 != "Unknown") {
        stream << "• IMEI 2:       " << deviceInfo.imei2 << "\n";
    }

    if (!deviceInfo.serial.isEmpty() && deviceInfo.serial != "Unknown") {
        stream << "• SERIAL:      " << deviceInfo.serial << "\n";
    }

    stream << "\n";

    // Дополнительная информация из EWC файла
    QString ewcFile = path + "/device.ewc";
    if (QFile::exists(ewcFile)) {
        stream << "Oxygen Forensic Project Information\n";
        stream << "══════════════════════════════════════════════\n\n";

        // Парсим EWC файл для получения дополнительной информации
        DeviceInfo ewcInfo = DeviceInfoParser::parseEwcFile(ewcFile);

        // Выводим информацию из EWC
        if (!ewcInfo.internalModel.isEmpty() && ewcInfo.internalModel != "Unknown") {
            stream << "• Internal Model: " << ewcInfo.internalModel << "\n";
        }

        if (!ewcInfo.extractionMethod.isEmpty() && ewcInfo.extractionMethod != "Unknown") {
            stream << "• Extraction:     " << ewcInfo.extractionMethod << "\n";
        }

        if (ewcInfo.extractionTime.isValid()) {
            stream << "• Extracted:      " << ewcInfo.extractionTime.toString("yyyy-MM-dd HH:mm:ss") << "\n";
        }

        // Читаем и выводим сырой текст EWC файла
        stream << "\nRaw EWC File Contents:\n";
        stream << "══════════════════════════════════════════════\n\n";

        QString ewcRawInfo = DeviceInfoParser::readEwcFile(ewcFile);
        if (!ewcRawInfo.isEmpty()) {
            stream << ewcRawInfo << "\n";
        }
    }


    // Информация о найденных файлах
    stream << "\nDirectory Contents\n";
    stream << "══════════════════════════════════════════════\n\n";

    QDir dir(path);
    QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    int totalFiles = files.size();
    int imageFiles = 0;
    qint64 totalSize = 0;

    for (const QString& file : files) {
        QFileInfo fileInfo(dir.absoluteFilePath(file));
        totalSize += fileInfo.size();

        if (file.endsWith(".bin", Qt::CaseInsensitive) ||
            file.endsWith(".img", Qt::CaseInsensitive) ||
            file.endsWith(".ewc", Qt::CaseInsensitive)) {
            imageFiles++;
        }
    }



    stream << "• Total files:    " << totalFiles << "\n";
    stream << "• Image files:    " << imageFiles << "\n";
    stream << "• Total size:     " << formatFileSize(totalSize) << "\n";



    // Проверяем наличие ключей шифрования
    QString keysFile = path + "/keys.json";
    if (QFile::exists(keysFile)) {
        stream << "\n📋 All Loaded Encryption Keys:\n";
        stream << "══════════════════════════════════════════════\n\n";

        QFile file(keysFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray content = file.readAll(); // Читаем весь файл
            file.close();

            QJsonDocument doc = QJsonDocument::fromJson(content);
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject keys = doc.object();

                // Список ключей в нужном порядке
                const QVector<QPair<QString, QString>> keyList = {
                    {"ChainType", "ChainType"},
                    {"MTK_CID", "MTK_CID"},
                    {"MTK_FDEKEY", "MTK_FDEKEY"},
                    {"MTK_HRID", "MTK_HRID"},
                    {"MTK_ITRUSTEE", "MTK_ITRUSTEE"},
                    {"MTK_ME_ID", "MTK_ME_ID"},
                    {"MTK_RID", "MTK_RID"},
                    {"MTK_RPMB2KEY", "MTK_RPMB2KEY"},
                    {"MTK_RPMBKEY", "MTK_RPMBKEY"},
                    {"MTK_SOCID", "MTK_SOCID"}
                };

                // Выводим каждый ключ в нужном формате
                for (const auto& keyPair : keyList) {
                    QString jsonKey = keyPair.first;
                    QString displayKey = keyPair.second;

                    if (keys.contains(jsonKey)) {
                        QString hexValue = keys[jsonKey].toString();
                        int byteSize = hexValue.length() / 2;

                        // Форматируем вывод в зависимости от размера
                        QString displayHex;
                        if (hexValue.length() <= 32) {
                            // Для ключей до 16 байт показываем полностью
                            displayHex = hexValue;
                        } else {
                            // Для длинных ключей показываем начало и конец
                            displayHex = hexValue.left(16) + "..." + hexValue.right(16);
                        }

                        stream << QString("    • %1: %2 (%3 bytes)\n")
                                      .arg(displayKey, -12)
                                      .arg(displayHex)
                                      .arg(byteSize);
                    }
                }

                // Показываем дополнительные ключи, если есть
                QJsonObject::iterator it;
                for (it = keys.begin(); it != keys.end(); ++it) {
                    QString keyName = it.key();
                    bool isStandardKey = false;

                    // Проверяем, не стандартный ли это ключ
                    for (const auto& keyPair : keyList) {
                        if (keyName == keyPair.first) {
                            isStandardKey = true;
                            break;
                        }
                    }

                    // Если это нестандартный ключ, показываем его
                    if (!isStandardKey) {
                        QString hexValue = it.value().toString();
                        int byteSize = hexValue.length() / 2;

                        QString displayHex;
                        if (hexValue.length() <= 32) {
                            displayHex = hexValue;
                        } else {
                            displayHex = hexValue.left(16) + "..." + hexValue.right(16);
                        }

                        stream << QString("    • %1: %2 (%3 bytes)\n")
                                      .arg(keyName, -12)
                                      .arg(displayHex)
                                      .arg(byteSize);
                    }
                }

            } else {
                // Если не JSON, показываем содержимое как текст
                stream << "  • Error: Cannot parse keys.json as valid JSON\n";
                stream << "  • Showing raw content:\n";

                QString keyInfo = QString::fromUtf8(content.left(512));
                stream << "    " << keyInfo.replace("\n", "\n    ") << "\n";

                if (content.size() > 512) {
                    stream << "    ... (truncated)\n";
                }
            }
        } else {
            stream << "  • Error: Cannot open keys.json for reading\n";
        }

        // Информация о файле ключей
        QFileInfo keyFileInfo(keysFile);
        stream << "\n📄 Keys File Information:\n";
                stream << QString("  - File: %1\n").arg(keyFileInfo.fileName());

    }

    ui->infoText->setPlainText(infoText);
}

// Вспомогательный метод для форматирования размера файла
QString MainWindow::formatFileSize(qint64 bytes)
{
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 2);
    } else if (bytes < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 2);
    } else {
        return QString("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
    }
}
