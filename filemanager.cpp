#include "filemanager.h"
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QStorageInfo>
#include <algorithm>

FileManager::FileManager(QObject *parent)
    : QObject(parent)
{
}

bool FileManager::extractPartition(const QString &sourcePath, quint64 offset,
                                   quint64 size, const QString &outputPath)
{
    qDebug() << "Извлечение раздела:";
    qDebug() << "  Источник:" << sourcePath;
    qDebug() << "  Смещение:" << offset;
    qDebug() << "  Размер:" << size;
    qDebug() << "  Выходной файл:" << outputPath;

    // Проверка существования исходного файла
    if (!fileExists(sourcePath)) {
        qWarning() << "Исходный файл не существует:" << sourcePath;
        return false;
    }

    // Проверка свободного места
    QString outputDir = QFileInfo(outputPath).absolutePath();
    quint64 freeSpace = getFreeSpace(outputDir);
    if (freeSpace < size) {
        qWarning() << "Недостаточно свободного места:" << freeSpace << "из" << size << "требуется";
        return false;
    }

    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Не удалось открыть исходный файл:" << sourcePath;
        return false;
    }

    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Не удалось открыть выходной файл:" << outputPath;
        sourceFile.close();
        return false;
    }

    // Переходим к смещению
    if (!sourceFile.seek(offset)) {
        qWarning() << "Не удалось перейти к смещению:" << offset;
        sourceFile.close();
        outputFile.close();
        return false;
    }

    // Извлекаем данные
    const qint64 bufferSize = 64 * 1024; // 64KB буфер
    quint64 remaining = size;
    quint64 totalRead = 0;

    while (remaining > 0) {
        // Явное приведение типов для qMin
        qint64 bytesToRead = std::min(static_cast<qint64>(bufferSize),
                                      static_cast<qint64>(remaining));

        QByteArray buffer = sourceFile.read(bytesToRead);
        if (buffer.isEmpty()) {
            qWarning() << "Ошибка чтения на позиции" << sourceFile.pos();
            break;
        }

        qint64 written = outputFile.write(buffer);
        if (written != buffer.size()) {
            qWarning() << "Ошибка записи, записано" << written << "из" << buffer.size() << "байт";
            break;
        }

        remaining -= buffer.size();
        totalRead += buffer.size();

        // Отправляем сигнал прогресса
        if (size > 0) {
            int percent = static_cast<int>((totalRead * 100) / size);
            emit progressChanged(percent, 100, QString("Извлечение: %1%").arg(percent));
        }

        // Прогресс каждые 10MB
        if (totalRead % (10 * 1024 * 1024) == 0) {
            qDebug() << "Извлечено:" << totalRead << "/" << size << "байт";
        }
    }

    sourceFile.close();
    outputFile.close();

    bool success = (totalRead == size);
    if (success) {
        qDebug() << "Извлечение успешно завершено:" << totalRead << "байт";
        emit progressChanged(100, 100, "Извлечение завершено");
    } else {
        qWarning() << "Извлечение неудачно:" << totalRead << "из" << size << "байт";
        emit progressChanged(0, 100, "Ошибка извлечения");
    }

    return success;
}

bool FileManager::copyFile(const QString &source, const QString &destination)
{
    if (!fileExists(source)) {
        return false;
    }

    return QFile::copy(source, destination);
}

bool FileManager::createDirectory(const QString &path)
{
    QDir dir;
    return dir.mkpath(path);
}

bool FileManager::fileExists(const QString &path)
{
    return QFileInfo::exists(path);
}

quint64 FileManager::getFileSize(const QString &path)
{
    QFileInfo fileInfo(path);
    if (fileInfo.exists()) {
        return static_cast<quint64>(fileInfo.size());
    }
    return 0;
}

quint64 FileManager::getFreeSpace(const QString &path)
{
    QStorageInfo storage(path);
    if (storage.isValid()) {
        return static_cast<quint64>(storage.bytesFree());
    }
    return 0;
}

bool FileManager::extractWithBuffer(QFile &sourceFile, QFile &outputFile,
                                    quint64 offset, quint64 size)
{
    if (!sourceFile.seek(offset)) {
        return false;
    }

    const qint64 bufferSize = 64 * 1024;
    quint64 remaining = size;

    while (remaining > 0) {
        // Явное приведение типов
        qint64 bytesToRead = std::min(static_cast<qint64>(bufferSize),
                                      static_cast<qint64>(remaining));

        QByteArray buffer = sourceFile.read(bytesToRead);
        if (buffer.isEmpty()) {
            return false;
        }

        if (outputFile.write(buffer) != buffer.size()) {
            return false;
        }

        remaining -= buffer.size();
    }

    return true;
}
