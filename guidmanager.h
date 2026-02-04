#ifndef GUIDMANAGER_H
#define GUIDMANAGER_H

#include <QObject>
#include <QMap>
#include <QUuid>
#include <QString>

/**
 * @class GuidManager
 * @brief Менеджер для работы с GUID разделами
 *
 * Этот класс содержит полную карту GUID для идентификации
 * типов разделов различных устройств и платформ.
 */
class GuidManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор класса
     * @param parent Родительский объект Qt
     */
    explicit GuidManager(QObject *parent = nullptr);

    /**
     * @brief Инициализирует карту GUID
     * @return Карта GUID->Описание раздела
     */
    QMap<QUuid, QString> initializeGuidMap();

    /**
     * @brief Получает описание раздела по GUID
     * @param guid UUID раздела
     * @return Описание раздела или "Unknown Partition"
     */
    QString getPartitionDescription(const QUuid &guid);

    /**
     * @brief Получает описание раздела по строковому GUID
     * @param guidString Строковое представление GUID
     * @return Описание раздела или "Unknown Partition"
     */
    QString getPartitionDescription(const QString &guidString);

    /**
     * @brief Проверяет, является ли раздел Android разделом
     * @param guid GUID раздела
     * @return true если это Android раздел
     */
    bool isAndroidPartition(const QUuid &guid);

    /**
     * @brief Проверяет, является ли раздел динамическим
     * @param guid GUID раздела
     * @return true если это динамический раздел (Super)
     */
    bool isDynamicPartition(const QUuid &guid);

private:
    QMap<QUuid, QString> m_guidMap; ///< Карта GUID->Описание
};

#endif // GUIDMANAGER_H
