#ifndef PARTITIONANALYZER_H
#define PARTITIONANALYZER_H

#include <QString>
#include <vector>
#include <cstdint>

/**
 * @brief Структура для хранения информации о разделе
 */
struct PartitionInfo {
    QString name;           // Имя раздела
    QString guid;           // GUID раздела
    quint64 size;           // Размер раздела в байтах
    QString type;           // Тип раздела
    quint64 offset;         // Смещение в файле

    // Конструктор по умолчанию
    PartitionInfo() : size(0), offset(0) {}

    // Конструктор с параметрами
    PartitionInfo(const QString& n, const QString& g, quint64 s, const QString& t, quint64 o)
        : name(n), guid(g), size(s), type(t), offset(o) {}
};

/**
 * @brief Класс для анализа разделов в образах дисков
 */
class PartitionAnalyzer
{
public:
    PartitionAnalyzer();
    ~PartitionAnalyzer() = default;

    /**
     * @brief Анализировать разделы в файле образа
     * @param filePath Путь к файлу образа
     * @return true если анализ успешен
     */
    bool analyzePartitions(const QString &filePath);

    /**
     * @brief Получить список найденных разделов
     * @return Вектор с информацией о разделах
     */
    std::vector<PartitionInfo> getPartitions() const;

    /**
     * @brief Проверить, является ли файл образом диска с разделами
     * @param filePath Путь к файлу
     * @return true если это образ с разделами
     */
    static bool isDiskImage(const QString &filePath);

    /**
     * @brief Получить информацию о MBR/GPT
     * @return Информация о таблице разделов
     */
    QString getPartitionTableInfo() const;

    /**
     * @brief Получить последнюю ошибку
     */
    QString getLastError() const { return m_lastError; }

private:
    /**
     * @brief Создать реалистичные тестовые разделы
     */
    void createRealisticTestPartitions(quint64 fileSize);



    bool parseMbrPartitions(const QByteArray &data, quint64 fileSize);
    bool parseGptPartitions(const QByteArray &data, quint64 sectorSize, quint64 fileSize);

    /**
     * @brief Парсинг GPT таблицы разделов
     */
    bool parseGptPartitions(const QByteArray &data, quint64 sectorSize);

    /**
     * @brief Парсинг MBR таблицы разделов
     */
    bool parseMbrPartitions(const QByteArray &data);

    /**
     * @brief Чтение GUID из данных
     */
    QString readGuid(const QByteArray &data, int offset) const;

    /**
     * @brief Определить тип раздела по коду
     */
    QString getPartitionType(quint8 typeCode) const;

    /**
     * @brief Определить тип раздела по GUID
     */
    QString getPartitionTypeFromGuid(const QString &guid) const;

    /**
     * @brief Создать тестовые разделы для отладки
     */
    void createTestPartitions();

    std::vector<PartitionInfo> m_partitions;
    QString m_partitionTableInfo;
    QString m_lastError;
};

#endif // PARTITIONANALYZER_H
