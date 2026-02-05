#ifndef GUIDMANAGER_H
#define GUIDMANAGER_H

#include <QObject>
#include <QTableWidget>

/**
 * @brief Класс для работы с GUID (Globally Unique Identifier)
 * Предоставляет информацию о типах разделов по их GUID
 */
class GuidManager : public QObject
{
    Q_OBJECT

public:
    explicit GuidManager(QObject *parent = nullptr);
    ~GuidManager() = default;

    /**
     * @brief Получить описание GUID
     * @param guid GUID для анализа
     * @return Описание типа раздела
     */
    QString getGuidDescription(const QString &guid) const;

    /**
     * @brief Отобразить детальную информацию о GUID в таблице
     * @param guid GUID для отображения
     * @param tableWidget Указатель на таблицу
     */
    void displayGuidDetails(const QString &guid, QTableWidget *tableWidget);

    /**
     * @brief Добавить GUID в базу данных
     * @param guid GUID
     * @param type Тип раздела
     * @param description Описание
     */
    void addGuid(const QString &guid, const QString &type, const QString &description);

    /**
     * @brief Проверить, существует ли GUID в базе данных
     */
    bool hasGuid(const QString &guid) const;

private:
    /**
     * @brief Инициализация базы данных GUID
     */
    void initGuidDatabase();

    /**
     * @brief Добавить метаданные GUID в таблицу
     */
    void addGuidMetadata(const QString &guid, QTableWidget *tableWidget);

    /**
     * @brief Форматировать GUID из байтов
     */
    QString formatGuid(const unsigned char *bytes) const;

    /**
     * @brief База данных GUID: GUID -> (Тип, Описание)
     */
    QMap<QString, QPair<QString, QString>> m_guidMap;
};

#endif // GUIDMANAGER_H
