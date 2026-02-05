#ifndef PARTITIONANALYZER_H
#define PARTITIONANALYZER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QDebug>

/**
 * @brief Структура для хранения информации о разделе диска/образа
 */
struct PartitionInfo {
    QString name;           ///< Имя раздела (например: "boot", "system")
    qint64 offset;          ///< Смещение раздела от начала файла в байтах
    qint64 size;            ///< Размер раздела в байтах
    QString type;           ///< Тип раздела (например: "ext4", "android_boot")
    QString guid;           ///< GUID раздела (для GPT таблицы)
    bool isMounted;         ///< Флаг, указывающий смонтирован ли раздел
    QString mountPoint;     ///< Точка монтирования (если раздел смонтирован)

    // Дополнительные поля для подробной информации
    QString typeGuid;       ///< GUID типа раздела
    qint64 startLba;        ///< Начальный LBA
    qint64 endLba;          ///< Конечный LBA
    quint64 attributes;     ///< Атрибуты раздела

    // Конструктор для удобного создания
    PartitionInfo(const QString& n = "", qint64 off = 0, qint64 sz = 0,
                  const QString& t = "", const QString& g = "", bool mounted = false,
                  const QString& mp = "", const QString& tg = "",
                  qint64 slba = 0, qint64 elba = 0, quint64 attr = 0)
        : name(n), offset(off), size(sz), type(t), guid(g),
        isMounted(mounted), mountPoint(mp), typeGuid(tg),
        startLba(slba), endLba(elba), attributes(attr) {}
};

/**
 * @brief Класс для анализа таблиц разделов диска/образа
 *
 * Класс обеспечивает анализ GPT (GUID Partition Table) и MBR (Master Boot Record)
 * таблиц разделов. Поддерживает создание тестовых разделов для отладки
 * и извлечение детальной информации о каждом разделе.
 *
 * @note Все методы класса потокобезопасны и могут быть вызваны из разных потоков.
 */
class PartitionAnalyzer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор анализатора разделов
     * @param parent Родительский объект QObject для управления памятью
     */
    explicit PartitionAnalyzer(QObject *parent = nullptr);

    /**
     * @brief Деструктор анализатора разделов
     */
    ~PartitionAnalyzer() override;

    /**
     * @brief Анализирует файл на наличие таблиц разделов
     * @param filePath Путь к файлу для анализа
     * @return true если анализ выполнен успешно, false в случае ошибки
     */
    bool analyzePartitions(const QString &filePath);

    /**
     * @brief Создает реалистичные тестовые разделы, имитирующие Android устройство
     * @param fileSize Размер анализируемого файла в байтах
     */
    void createRealisticTestPartitions(qint64 fileSize);

    /**
     * @brief Создает простые тестовые разделы для отладки
     * @param fileSize Размер анализируемого файла в байтах
     */
    void createTestPartitions(qint64 fileSize);

    /**
     * @brief Возвращает список найденных разделов
     * @return Константная ссылка на вектор с информацией о разделах
     */
    const QVector<PartitionInfo> &getPartitions() const { return m_partitions; }

    /**
     * @brief Получает размер файла
     * @param filePath Путь к файлу
     * @return Размер файла в байтах или -1 в случае ошибки
     */
    qint64 getFileSize(const QString &filePath);

    /**
     * @brief Включает/отключает подробное логирование
     * @param enabled true для включения подробного логирования
     */
    void setVerboseLogging(bool enabled) { m_verboseLogging = enabled; }

    /**
     * @brief Возвращает последнее сообщение об ошибке
     */
    QString lastError() const { return m_lastError; }

signals:
    /**
     * @brief Сигнал об обновлении прогресса анализа
     * @param progress Текущий прогресс от 0 до 100
     */
    void progressUpdated(int progress);

    /**
     * @brief Сигнал о завершении анализа
     * @param success true если анализ успешен, false в случае ошибки
     */
    void analysisComplete(bool success);

    /**
     * @brief Сигнал об ошибке анализа
     * @param errorMessage Сообщение об ошибке на русском языке
     */
    void errorOccurred(const QString &errorMessage);

    /**
     * @brief Сигнал с информационным сообщением для лога
     * @param message Информационное сообщение
     */
    void logMessage(const QString &message);

private:

    /**
     * @brief Находит смещение GPT заголовка в файле
     * @param file Открытый файл для поиска
     * @return Смещение GPT заголовка или -1 если не найден
     */
    qint64 findGPTHeaderOffset(QFile &file);


    /**
     * @brief Анализирует GPT (GUID Partition Table) структуру
     * @param file Открытый файл с подтвержденной GPT сигнатурой
     * @return true если анализ GPT выполнен успешно, иначе false
     */
    bool analyzeGPT(QFile &file);

    /**
     * @brief Анализирует MBR (Master Boot Record) структуру
     * @param file Открытый файл с подтвержденной MBR сигнатурой
     * @return true если анализ MBR выполнен успешно, иначе false
     */
    bool analyzeMBR(QFile &file);

    /**
     * @brief Проверяет наличие GPT сигнатуры в файле
     * @param file Открытый файл для проверки
     * @return true если найдена сигнатура GPT, иначе false
     */
    bool checkGPTSignature(QFile &file);

    /**
     * @brief Проверяет наличие MBR сигнатуры в файле
     * @param file Открытый файл для проверки
     * @return true если найдена сигнатура MBR, иначе false
     */
    bool checkMBRSignature(QFile &file);

    /**
     * @brief Читает UTF-16 строку из файла
     * @param file Открытый файл для чтения
     * @param maxLength Максимальное количество символов UTF-16 для чтения
     * @return Преобразованная QString или пустая строка в случае ошибки
     */
    QString readUTF16String(QFile &file, int maxLength);

    /**
     * @brief Преобразует GUID байты в строковый формат
     * @param data Байты GUID
     * @param size Размер данных (должен быть 16 для GUID)
     * @return GUID в формате строки
     */
    QString bytesToGUID(const char* data, int size);

    /**
     * @brief Получает имя типа раздела по его GUID
     * @param typeGuid GUID типа раздела
     * @return Человеко-читаемое имя типа раздела
     */
    QString getPartitionTypeName(const QString &typeGuid);

    /**
     * @brief Логирует сообщение с временной меткой
     * @param message Сообщение для логирования
     */
    void log(const QString &message);

    /**
     * @brief Логирует ошибку
     * @param message Сообщение об ошибке
     */
    void logError(const QString &message);

private:
    QVector<PartitionInfo> m_partitions; ///< Список найденных разделов
    QString m_filePath;                  ///< Путь к текущему анализируемому файлу
    QString m_lastFilePath;              ///< Путь к последнему анализированному файлу
    QString m_lastError;                 ///< Последнее сообщение об ошибке
    bool m_verboseLogging;               ///< Флаг подробного логирования
    int m_sectorSize;                    ///< Размер сектора (обычно 512 байт)

    /**
     * @brief Проверяет перекрытие разделов
     */
    void checkPartitionOverlaps();


    QString formatFileSize(qint64 size);
    QString getPartitionTypeByName(const QString &name);
    bool checkProtectiveMBR(QFile &file);
};

#endif // PARTITIONANALYZER_H
