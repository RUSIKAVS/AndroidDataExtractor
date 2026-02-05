#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <cstdint>

/**
 * @brief Класс для управления файловыми операциями
 * Извлечение разделов, копирование файлов и т.д.
 */
class FileManager : public QObject
{
    Q_OBJECT

public:
    explicit FileManager(QObject *parent = nullptr);
    ~FileManager() = default;

    /**
     * @brief Извлечь раздел из образа
     * @param sourcePath Путь к исходному файлу
     * @param offset Смещение раздела в байтах
     * @param size Размер раздела в байтах
     * @param outputPath Путь для сохранения
     * @return true если извлечение успешно
     */
    bool extractPartition(const QString &sourcePath, quint64 offset,
                          quint64 size, const QString &outputPath);

    /**
     * @brief Копировать файл
     */
    bool copyFile(const QString &source, const QString &destination);

    /**
     * @brief Создать директорию
     */
    bool createDirectory(const QString &path);

    /**
     * @brief Проверить существование файла
     */
    static bool fileExists(const QString &path);

    /**
     * @brief Получить размер файла
     */
    static quint64 getFileSize(const QString &path);

    /**
     * @brief Получить свободное место на диске
     */
    static quint64 getFreeSpace(const QString &path);

signals:
    /**
     * @brief Сигнал прогресса операции
     * @param current Текущее значение
     * @param total Общее значение
     * @param message Сообщение о статусе
     */
    void progressChanged(int current, int total, const QString &message);

private:
    /**
     * @brief Извлечение с буферизацией
     */
    bool extractWithBuffer(QFile &sourceFile, QFile &outputFile,
                           quint64 offset, quint64 size);
};

#endif // FILEMANAGER_H
