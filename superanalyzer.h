#ifndef SUPERANALYZER_H
#define SUPERANALYZER_H

#include <QObject>
#include <QTreeWidget>
#include <QFile>
#include <vector>
#include <memory>

/**
 * @brief Структура для хранения информации о разделе внутри SUPER
 */
struct SuperPartitionInfo {
    QString name;           // Имя раздела
    QString guid;           // GUID раздела
    quint64 offset;         // Смещение внутри super
    quint64 size;           // Размер раздела
    quint32 partitionType;  // Тип раздела
    bool isLogical;         // Логический раздел
    bool isSparse;          // Sparse образ
};

/**
 * @brief Анализатор динамических разделов SUPER
 * Поддерживает Android Dynamic Partitions
 */
class SuperAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit SuperAnalyzer(QObject *parent = nullptr);
    ~SuperAnalyzer();

    /**
     * @brief Анализ файла super
     * @param filePath Путь к файлу super
     * @param offset Смещение в файле (если super внутри другого образа)
     * @return true если анализ успешен
     */
    bool analyzeSuper(const QString &filePath, quint64 offset = 0);

    /**
     * @brief Получить список разделов из super
     * @return Вектор с информацией о разделах
     */
    std::vector<SuperPartitionInfo> getPartitions() const;

    /**
     * @brief Извлечь раздел из super
     * @param partitionName Имя раздела
     * @param outputPath Путь для сохранения
     * @return true если извлечение успешно
     */
    bool extractPartition(const QString &partitionName, const QString &outputPath);

    /**
     * @brief Проверить, является ли файл super разделом
     * @param filePath Путь к файлу
     * @return true если это super раздел
     */
    static bool isSuperImage(const QString &filePath);

    /**
     * @brief Показать структуру super в виде дерева
     * @param treeWidget Указатель на виджет дерева
     */
    void populateTreeWidget(QTreeWidget *treeWidget);

private:
    /**
     * @brief Парсинг LP-метаданных (Android Logical Partitions)
     */
    bool parseLpMetadata();

    /**
     * @brief Парсинг LP-метаданных с учетом смещения
     */
    bool parseLpMetadataWithOffset(quint64 offset);

    /**
     * @brief Парсинг sparse образа
     */
    bool parseSparseImage(QFile &file, quint64 offset);

    /**
     * @brief Чтение GUID из бинарных данных
     */
    QString readGuid(const QByteArray &data, int offset);

    /**
     * @brief Декодировать тип раздела
     */
    QString decodePartitionType(quint32 type) const;

    /**
     * @brief Создать тестовые разделы для отладки
     */
    void createTestPartitions();

    /**
     * @brief Сгенерировать тестовый GUID для раздела
     */
    QString generateTestGuid(const QString &partitionName);

    QString m_filePath;
    std::vector<SuperPartitionInfo> m_partitions;
    QByteArray m_superData;
    bool m_isValid;
    quint64 m_fileOffset;
};

#endif // SUPERANALYZER_H
