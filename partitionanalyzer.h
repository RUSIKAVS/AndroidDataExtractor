#ifndef PARTITIONANALYZER_H
#define PARTITIONANALYZER_H

#include <QObject>
#include <QFile>
#include <QVector>
#include <QString>
#include <QUuid>
#include <cstdint>

/**
 * @struct PartitionInfo
 * @brief Структура для хранения информации о разделе
 */
struct PartitionInfo
{
    QString name;                   ///< Имя раздела
    uint64_t startSector;           ///< Начальный сектор
    uint64_t sizeSectors;           ///< Размер в секторах
    uint64_t startByte;             ///< Начало в байтах
    uint64_t sizeBytes;             ///< Размер в байтах
    QUuid guid;                     ///< GUID раздела
    QString type;                   ///< Тип раздела (MBR/GPT)
    QString fileSystem;             ///< Файловая система
    bool bootable;                  ///< Загрузочный раздел
    QString description;            ///< Описание из GUID

    PartitionInfo()
        : startSector(0), sizeSectors(0), startByte(0), sizeBytes(0),
        bootable(false) {}
};

/**
 * @struct DiskInfo
 * @brief Структура для хранения информации о диске/образе
 */
struct DiskInfo
{
    QString path;                   ///< Путь к файлу
    uint64_t totalSize;             ///< Общий размер в байтах
    QString partitionTable;         ///< Тип таблицы разделов (MBR/GPT/SUPER/RAW)
    uint32_t sectorSize;            ///< Размер сектора
    QVector<PartitionInfo> partitions; ///< Список разделов

    DiskInfo() : totalSize(0), sectorSize(512) {}
};

/**
 * @class PartitionAnalyzer
 * @brief Анализатор разделов диска/образа
 *
 * Этот класс анализирует образы дисков, определяет тип таблицы разделов
 * и извлекает информацию о разделах.
 */
class PartitionAnalyzer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор класса
     * @param parent Родительский объект Qt
     */
    explicit PartitionAnalyzer(QObject *parent = nullptr);

    /**
     * @brief Деструктор класса
     */
    ~PartitionAnalyzer();

    /**
     * @brief Анализирует образ диска
     * @param filePath Путь к файлу образа
     * @return Информация о диске
     */
    DiskInfo analyzeDisk(const QString &filePath);

    /**
     * @brief Читает данные из раздела
     * @param diskInfo Информация о диске
     * @param partitionIndex Индекс раздела
     * @param offset Смещение в разделе (байты)
     * @param size Размер данных для чтения (байты)
     * @return Массив байтов данных
     */
    QByteArray readPartitionData(const DiskInfo &diskInfo,
                                 int partitionIndex,
                                 uint64_t offset = 0,
                                 uint64_t size = 4096);

signals:
    /**
     * @brief Сигнал для отправки логов
     * @param message Сообщение лога
     */
    void logMessage(const QString &message);

    /**
     * @brief Сигнал об ошибке
     * @param errorMessage Сообщение об ошибке
     */
    void errorOccurred(const QString &errorMessage);

private:
    /**
     * @brief Определяет тип таблицы разделов
     * @param file Файл образа
     * @return Тип таблицы разделов
     */
    QString detectPartitionTableType(QFile &file);

    /**
     * @brief Анализирует MBR таблицу разделов
     * @param file Файл образа
     * @param diskInfo Информация о диске (обновляется)
     */
    void analyzeMBR(QFile &file, DiskInfo &diskInfo);

    /**
     * @brief Анализирует GPT таблицу разделов
     * @param file Файл образа
     * @param diskInfo Информация о диске (обновляется)
     */
    void analyzeGPT(QFile &file, DiskInfo &diskInfo);

    /**
     * @brief Анализирует Super раздел (Android динамические разделы)
     * @param file Файл образа
     * @param diskInfo Информация о диске (обновляется)
     */
    void analyzeSuper(QFile &file, DiskInfo &diskInfo);

    /**
     * @brief Анализирует RAW раздел (без таблицы разделов)
     * @param file Файл образа
     * @param diskInfo Информация о диске (обновляется)
     */
    void analyzeRaw(QFile &file, DiskInfo &diskInfo);

    /**
     * @brief Определяет размер сектора для GPT
     * @param file Файл образа
     * @return Размер сектора или 0 если не определен
     */
    uint32_t detectGptSectorSize(QFile &file);

    /**
     * @brief Читает GUID из массива байтов
     * @param data Массив байтов
     * @param offset Смещение
     * @return GUID
     */
    QUuid readGuid(const QByteArray &data, int offset);

    /**
     * @brief Преобразует байты в little-endian uint32
     */
    uint32_t bytesToUint32(const QByteArray &data, int offset);

    /**
     * @brief Преобразует байты в little-endian uint64
     */
    uint64_t bytesToUint64(const QByteArray &data, int offset);

    QFile *m_currentFile;           ///< Текущий открытый файл
};

#endif // PARTITIONANALYZER_H
