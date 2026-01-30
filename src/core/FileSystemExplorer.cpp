#include "FileSystemExplorer.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QProcess>
#include <QStorageInfo>
#include <QDebug>
#include <QDesktopServices>
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>
#include <QtConcurrent>
#include <QProgressDialog>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winioctl.h>
#endif

FileSystemExplorer::FileSystemExplorer(QWidget *parent)
    : QWidget(parent)
    , fileModel(nullptr)
    , treeView(nullptr)
    , fileInfoText(nullptr)
    , statusLabel(nullptr)
    , mounted(false)
{
    setupUI();
}

FileSystemExplorer::~FileSystemExplorer()
{
    if (mounted) {
        unmountPartition();
    }
}

void FileSystemExplorer::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Заголовок
    QLabel* titleLabel = new QLabel("📁 File System Explorer");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; margin: 5px;");
    mainLayout->addWidget(titleLabel);

    // Основной виджет с разделителем
    QSplitter* splitter = new QSplitter(Qt::Horizontal);

    // Левая панель - дерево файлов
    treeView = new QTreeView();
    treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    treeView->setAlternatingRowColors(true);
    treeView->setSortingEnabled(true);

    // Правая панель - информация о файле
    fileInfoText = new QTextEdit();
    fileInfoText->setReadOnly(true);
    fileInfoText->setFont(QFont("Consolas", 9));
    fileInfoText->setPlaceholderText("Select a file to view details...");

    splitter->addWidget(treeView);
    splitter->addWidget(fileInfoText);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);

    // Статус бар
    statusLabel = new QLabel("No partition mounted");
    statusLabel->setStyleSheet("color: #666; font-style: italic; padding: 5px;");
    statusLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    mainLayout->addWidget(statusLabel);

    // Панель инструментов
    QHBoxLayout* toolLayout = new QHBoxLayout();

    QPushButton* refreshBtn = new QPushButton("🔄 Refresh");
    QPushButton* extractBtn = new QPushButton("📤 Extract Selected");
    QPushButton* extractAllBtn = new QPushButton("📦 Extract All");
    QPushButton* unmountBtn = new QPushButton("⏏️ Unmount");

    connect(refreshBtn, &QPushButton::clicked, this, &FileSystemExplorer::refreshView);
    connect(extractBtn, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory");
        if (!dir.isEmpty()) {
            extractSelectedFiles(dir);
        }
    });
    connect(extractAllBtn, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory");
        if (!dir.isEmpty()) {
            extractAllFiles(dir);
        }
    });
    connect(unmountBtn, &QPushButton::clicked, this, &FileSystemExplorer::unmountPartition);

    toolLayout->addWidget(refreshBtn);
    toolLayout->addWidget(extractBtn);
    toolLayout->addWidget(extractAllBtn);
    toolLayout->addWidget(unmountBtn);
    toolLayout->addStretch();

    mainLayout->addLayout(toolLayout);

    // Подключение сигналов
    connect(treeView, &QTreeView::clicked, this, &FileSystemExplorer::onFileClicked);
    connect(treeView, &QTreeView::customContextMenuRequested,
            this, &FileSystemExplorer::onContextMenuRequested);

    // Инициализация модели (будет создана при монтировании)
    fileModel = nullptr;
    mounted = false;
}

void FileSystemExplorer::setupContextMenu()
{
    // Контекстное меню будет создаваться динамически
}

bool FileSystemExplorer::mountPartition(const AndroidPartition& partition,
                                       const QByteArray& decryptionKey)
{
    qDebug() << "Mounting partition:" << partition.name;

    // Сначала демонтируем, если уже что-то смонтировано
    if (mounted) {
        unmountPartition();
    }

    // Создаем временный файл с дешифрованными данными
    QString tempFile = QDir::tempPath() + "/android_partition_" +
                      QString::number(QDateTime::currentSecsSinceEpoch()) + ".bin";

    // Дешифруем раздел во временный файл
    if (!decryptionKey.isEmpty()) {
        QFile source(partition.filePath);
        QFile dest(tempFile);

        if (!source.open(QIODevice::ReadOnly) || !dest.open(QIODevice::WriteOnly)) {
            return false;
        }

        source.seek(partition.offset);

        // TODO: Реальная дешифровка
        // Пока просто копируем
        QByteArray buffer = source.read(partition.size);
        dest.write(buffer);

        source.close();
        dest.close();

        partitionPath = tempFile;
    } else {
        partitionPath = partition.filePath;
    }

    // Создаем точку монтирования
    currentMountPoint = createMountPoint();
    if (currentMountPoint.isEmpty()) {
        return false;
    }

    // Определяем тип файловой системы
    QString fsType = partition.type.toLower();

    bool mountSuccess = false;

    if (fsType.contains("ext4") || fsType.contains("ext3") || fsType.contains("ext2")) {
        mountSuccess = mountExt4(partitionPath, currentMountPoint);
    } else if (fsType.contains("f2fs")) {
        mountSuccess = mountF2fs(partitionPath, currentMountPoint);
    } else if (fsType.contains("erofs")) {
        mountSuccess = mountErofs(partitionPath, currentMountPoint);
    } else if (fsType.contains("fat") || fsType.contains("vfat")) {
        mountSuccess = mountFat(partitionPath, currentMountPoint);
    } else {
        qWarning() << "Unsupported filesystem:" << fsType;
        statusLabel->setText("❌ Unsupported filesystem: " + fsType);
        return false;
    }

    if (mountSuccess) {
        mounted = true;

        // Инициализируем модель файловой системы
        fileModel = new QFileSystemModel(this);
        fileModel->setRootPath(currentMountPoint);

        treeView->setModel(fileModel);
        treeView->setRootIndex(fileModel->index(currentMountPoint));
        treeView->setColumnWidth(0, 300);

        // Скрываем системные столбцы
        for (int i = 1; i < fileModel->columnCount(); ++i) {
            treeView->hideColumn(i);
        }

        statusLabel->setText("✅ Mounted: " + partition.name + " (" + fsType + ")");

        connect(treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
                [this](const QItemSelection& selected, const QItemSelection&) {
                    if (!selected.indexes().isEmpty()) {
                        QModelIndex index = selected.indexes().first();
                        QString filePath = fileModel->filePath(index);
                        showFileInfo(filePath);
                        emit fileSelected(filePath);
                    }
                });

        return true;
    } else {
        // Очищаем временные файлы
        if (!decryptionKey.isEmpty()) {
            QFile::remove(tempFile);
        }
        QDir().rmdir(currentMountPoint);
        return false;
    }
}

bool FileSystemExplorer::unmountPartition()
{
    if (!mounted || currentMountPoint.isEmpty()) {
        return true;
    }

#ifdef Q_OS_WIN
    // Для Windows используем diskpart или другие утилиты
    QProcess process;
    process.start("cmd", QStringList() << "/c" << "diskpart");

    if (process.waitForStarted()) {
        QString script = QString("select volume %1\ndetach volume\n")
                         .arg(currentMountPoint.right(1)); // Буква диска
        process.write(script.toUtf8());
        process.closeWriteChannel();
        process.waitForFinished();
    }
#else
    // Для Linux/Mac
    QProcess::execute("umount", QStringList() << currentMountPoint);
#endif

    // Удаляем точку монтирования
    QDir().rmdir(currentMountPoint);

    // Очищаем модель
    if (fileModel) {
        delete fileModel;
        fileModel = nullptr;
    }

    mounted = false;
    statusLabel->setText("Partition unmounted");

    return true;
}

QString FileSystemExplorer::createMountPoint()
{
#ifdef Q_OS_WIN
    // Для Windows создаем виртуальный диск
    QString mountPoint = "Z:\\"; // Используем Z: как временную букву

    // Проверяем, свободна ли буква Z
    DWORD drives = GetLogicalDrives();
    if (drives & (1 << ('Z' - 'A'))) {
        // Z занята, ищем свободную букву
        for (char letter = 'D'; letter <= 'Z'; letter++) {
            if (!(drives & (1 << (letter - 'A')))) {
                mountPoint = QString("%1:\\").arg(letter);
                break;
            }
        }
    }

    return mountPoint;
#else
    // Для Linux/Mac создаем временную директорию
    QString tempDir = QDir::tempPath() + "/android_mount_" +
                     QString::number(QDateTime::currentSecsSinceEpoch());

    if (QDir().mkpath(tempDir)) {
        return tempDir;
    }

    return QString();
#endif
}

bool FileSystemExplorer::mountExt4(const QString& partitionPath, const QString& mountPoint)
{
#ifdef Q_OS_WIN
    // Для Windows используем Ext2Fsd или подобные драйверы
    // Временно возвращаем false, так как требуется установка драйвера
    QMessageBox::information(nullptr, "Windows Limitation",
                            "Ext4 mounting requires Ext2Fsd driver.\n"
                            "Please install it from: http://www.ext2fsd.com/");
    return false;
#else
    return executeMountCommand("mount",
                               QStringList() << "-o" << "ro,loop"
                                             << partitionPath << mountPoint);
#endif
}

bool FileSystemExplorer::mountF2fs(const QString& partitionPath, const QString& mountPoint)
{
#ifdef Q_OS_WIN
    QMessageBox::information(nullptr, "Windows Limitation",
                            "F2FS mounting is not supported on Windows.");
    return false;
#else
    return executeMountCommand("mount",
                               QStringList() << "-t" << "f2fs" << "-o" << "ro"
                                             << partitionPath << mountPoint);
#endif
}

bool FileSystemExplorer::mountErofs(const QString& partitionPath, const QString& mountPoint)
{
#ifdef Q_OS_WIN
    QMessageBox::information(nullptr, "Windows Limitation",
                            "EROFS mounting is not supported on Windows.");
    return false;
#else
    // Проверяем поддержку EROFS в ядре
    QProcess process;
    process.start("modprobe", QStringList() << "erofs");
    process.waitForFinished();

    return executeMountCommand("mount",
                               QStringList() << "-t" << "erofs" << "-o" << "ro"
                                             << partitionPath << mountPoint);
#endif
}

bool FileSystemExplorer::mountFat(const QString& partitionPath, const QString& mountPoint)
{
#ifdef Q_OS_WIN
    // В Windows FAT поддерживается нативно
    // Монтируем через Virtual Disk или используем raw доступ
    return true;
#else
    return executeMountCommand("mount",
                               QStringList() << "-t" << "vfat" << "-o" << "ro,umask=000"
                                             << partitionPath << mountPoint);
#endif
}

bool FileSystemExplorer::executeMountCommand(const QString& cmd, const QStringList& args)
{
    QProcess process;
    process.start(cmd, args);

    if (!process.waitForStarted()) {
        return false;
    }

    if (!process.waitForFinished(10000)) {
        process.kill();
        return false;
    }

    return (process.exitCode() == 0);
}

void FileSystemExplorer::onContextMenuRequested(const QPoint& pos)
{
    if (!mounted || !fileModel) return;

    QModelIndex index = treeView->indexAt(pos);
    if (!index.isValid()) return;

    QString filePath = fileModel->filePath(index);

    QMenu contextMenu;

    QAction* extractAction = contextMenu.addAction("📤 Extract");
    QAction* infoAction = contextMenu.addAction("ℹ️ Info");
    QAction* openAction = contextMenu.addAction("👁️ View");
    contextMenu.addSeparator();
    QAction* copyPathAction = contextMenu.addAction("📋 Copy Path");

    QAction* selectedAction = contextMenu.exec(treeView->viewport()->mapToGlobal(pos));

    if (selectedAction == extractAction) {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory");
        if (!dir.isEmpty()) {
            extractSelectedFiles(dir);
        }
    } else if (selectedAction == infoAction) {
        showFileInfo(filePath);
    } else if (selectedAction == openAction) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    } else if (selectedAction == copyPathAction) {
        QApplication::clipboard()->setText(filePath);
    }
}

void FileSystemExplorer::showFileInfo(const QString& filePath)
{
    QFileInfo info(filePath);

    // Преобразуем права доступа в читаемую строку
    QString permissionsStr;
    QFileDevice::Permissions perms = info.permissions();

    // Формат: drwxrwxrwx (как в Linux)
    if (info.isDir()) permissionsStr += "d";
    else if (info.isFile()) permissionsStr += "-";
    else permissionsStr += "?";

    // Владелец
    permissionsStr += (perms & QFileDevice::ReadOwner) ? "r" : "-";
    permissionsStr += (perms & QFileDevice::WriteOwner) ? "w" : "-";
    permissionsStr += (perms & QFileDevice::ExeOwner) ? "x" : "-";

    // Группа
    permissionsStr += (perms & QFileDevice::ReadGroup) ? "r" : "-";
    permissionsStr += (perms & QFileDevice::WriteGroup) ? "w" : "-";
    permissionsStr += (perms & QFileDevice::ExeGroup) ? "x" : "-";

    // Остальные
    permissionsStr += (perms & QFileDevice::ReadOther) ? "r" : "-";
    permissionsStr += (perms & QFileDevice::WriteOther) ? "w" : "-";
    permissionsStr += (perms & QFileDevice::ExeOther) ? "x" : "-";

    // Альтернативно, можно использовать числовой формат
    QString numericPerms = QString::number(static_cast<int>(perms), 8).right(3);

    QString fileInfo = QString(
                           "File: %1\n"
                           "Size: %2 bytes\n"
                           "Type: %3\n"
                           "Created: %4\n"
                           "Modified: %5\n"
                           "Permissions: %6 (%7)\n"
                           "Path: %8\n")
                           .arg(info.fileName())
                           .arg(info.size())
                           .arg(info.isDir() ? "Directory" : (info.isFile() ? "File" : "Other"))
                           .arg(info.birthTime().isValid() ?
                                    info.birthTime().toString("yyyy-MM-dd HH:mm:ss") : "Unknown")
                           .arg(info.lastModified().toString("yyyy-MM-dd HH:mm:ss"))
                           .arg(permissionsStr)
                           .arg(numericPerms)
                           .arg(info.absoluteFilePath());

    // Добавляем дополнительную информацию для определенных типов файлов
    QString lowerName = info.fileName().toLower();

    if (lowerName.endsWith(".db") || lowerName.endsWith(".sqlite") ||
        lowerName.endsWith(".sqlite3")) {
        fileInfo += "\nType: SQLite database";
    } else if (lowerName.endsWith(".xml") || lowerName.endsWith(".txt")) {
        fileInfo += "\nType: Text file";
    } else if (lowerName.endsWith(".jpg") || lowerName.endsWith(".jpeg") ||
               lowerName.endsWith(".png") || lowerName.endsWith(".gif")) {
        fileInfo += "\nType: Image file";
    } else if (lowerName.endsWith(".apk")) {
        fileInfo += "\nType: Android application";
    } else if (lowerName.endsWith(".so")) {
        fileInfo += "\nType: Shared library";
    } else if (lowerName.endsWith(".odex") || lowerName.endsWith(".vdex")) {
        fileInfo += "\nType: Android optimized dex";
    }

    fileInfoText->setPlainText(fileInfo);
}

void FileSystemExplorer::extractSelectedFiles(const QString& outputDir)
{
    if (!mounted) return;

    QModelIndexList selected = treeView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    QStringList files;
    for (const QModelIndex& index : selected) {
        if (index.column() == 0) { // Только первый столбец
            files.append(fileModel->filePath(index));
        }
    }

    files.removeDuplicates();

    QProgressDialog progress("Extracting files...", "Cancel", 0, files.size(), this);
    progress.setWindowModality(Qt::WindowModal);

    int successCount = 0;
    for (int i = 0; i < files.size(); ++i) {
        if (progress.wasCanceled()) break;

        QString sourceFile = files[i];
        QString relativePath = QDir(currentMountPoint).relativeFilePath(sourceFile);
        QString destFile = outputDir + "/" + relativePath;

        progress.setLabelText(QString("Extracting: %1").arg(QFileInfo(sourceFile).fileName()));
        progress.setValue(i);

        // Создаем целевую директорию если нужно
        QFileInfo destInfo(destFile);
        QDir().mkpath(destInfo.path());

        if (QFile::copy(sourceFile, destFile)) {
            successCount++;
        }

        emit extractionProgress((i * 100) / files.size());
    }

    progress.close();

    QMessageBox::information(this, "Extraction Complete",
                            QString("Extracted %1 of %2 files successfully.")
                            .arg(successCount).arg(files.size()));

    emit extractionFinished(successCount > 0);
}

void FileSystemExplorer::extractAllFiles(const QString& outputDir)
{
    if (!mounted) return;

    // Рекурсивно собираем все файлы
    QStringList allFiles;
    QDirIterator it(currentMountPoint, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().isFile()) {
            allFiles.append(it.filePath());
        }
    }

    QProgressDialog progress("Extracting all files...", "Cancel", 0, allFiles.size(), this);
    progress.setWindowModality(Qt::WindowModal);

    int successCount = 0;
    for (int i = 0; i < allFiles.size(); ++i) {
        if (progress.wasCanceled()) break;

        QString sourceFile = allFiles[i];
        QString relativePath = QDir(currentMountPoint).relativeFilePath(sourceFile);
        QString destFile = outputDir + "/" + relativePath;

        progress.setLabelText(QString("Extracting: %1").arg(QFileInfo(sourceFile).fileName()));
        progress.setValue(i);

        // Создаем целевую директорию если нужно
        QFileInfo destInfo(destFile);
        QDir().mkpath(destInfo.path());

        if (QFile::copy(sourceFile, destFile)) {
            successCount++;
        }

        emit extractionProgress((i * 100) / allFiles.size());
    }

    progress.close();

    QMessageBox::information(this, "Extraction Complete",
                            QString("Extracted %1 files successfully.")
                            .arg(successCount));

    emit extractionFinished(successCount > 0);
}

QStringList FileSystemExplorer::getSelectedFiles() const
{
    QStringList files;

    if (!mounted || !treeView->selectionModel()) {
        return files;
    }

    QModelIndexList selected = treeView->selectionModel()->selectedIndexes();
    for (const QModelIndex& index : selected) {
        if (index.column() == 0) {
            files.append(fileModel->filePath(index));
        }
    }

    return files;
}

QStringList FileSystemExplorer::getAllFiles() const
{
    QStringList files;

    if (!mounted) {
        return files;
    }

    QDirIterator it(currentMountPoint, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().isFile()) {
            files.append(it.filePath());
        }
    }

    return files;
}

bool FileSystemExplorer::supportsFilesystem(const QString& fsType)
{
    QString lower = fsType.toLower();

    return lower.contains("ext") ||
           lower.contains("f2fs") ||
           lower.contains("erofs") ||
           lower.contains("fat") ||
           lower.contains("vfat") ||
           lower.contains("ntfs");
}

QString FileSystemExplorer::getMountedPath() const
{
    return currentMountPoint;
}

bool FileSystemExplorer::isMounted() const
{
    return mounted;
}

void FileSystemExplorer::refreshView()
{
    if (mounted && fileModel) {
        // Способ 1: Сбросить модель (простой, но эффективный)
        treeView->reset();

        // Способ 2: Обновить конкретный индекс
        QModelIndex rootIndex = fileModel->index(currentMountPoint);
        if (rootIndex.isValid()) {
            // Используем dataChanged сигнал для принудительного обновления
            emit fileModel->dataChanged(rootIndex, rootIndex);

            // Или можно использовать layoutChanged
            emit fileModel->layoutChanged();
        }

        // Способ 3: Переустановить корневой индекс
        treeView->setRootIndex(fileModel->index(currentMountPoint));

        statusLabel->setText("View refreshed");

        qDebug() << "File system view refreshed";
    } else {
        qDebug() << "Cannot refresh: no partition mounted";
    }
}

void FileSystemExplorer::onFileClicked(const QModelIndex& index)
{
    if (!mounted || !fileModel || !index.isValid()) {
        return;
    }

    QString filePath = fileModel->filePath(index);

    // Показываем информацию о файле
    showFileInfo(filePath);

    // Эмитируем сигнал
    emit fileSelected(filePath);

    qDebug() << "File clicked:" << filePath;
}
