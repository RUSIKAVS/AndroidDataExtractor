#pragma once

#include <QString>
#include <QMap>
#include <QJsonObject>
#include <QByteArray>
#include <QVariantMap>

// Парсинг Oxygen Forensic файлов
struct EwcInfo {
    QString contentType;
    QString extractionMethod;
    QDateTime extractionStart;
    QDateTime extractionEnd;
    QString internalModelName;
    QString productName;
    QString productVersion;
    QString deviceAlias;

    // Информация о разделах
    struct PartitionInfo {
        QString name;
        QString file;
        qint64 size;
        QString md5;
        QString sha1;
        QString sha256;
        QString sha3256;
    };

    QList<PartitionInfo> partitions;
    QString keyBagFile;
};



class KeyParser
{
public:
    struct MtkKeyInfo {
        QString key;           // ключ в hex
        QString description;   // описание ключа
    };

    bool parseKeysFile(const QString& filePath);
    QByteArray getKey(const QString& keyId) const;
    bool hasKey(const QString& keyId) const;

    // Получение всех ключей для отображения
    QMap<QString, QString> getAllKeys() const;

    // Специфичные MTK ключи
    QByteArray getRpmbKey() const;
    QByteArray getFdeKey() const;
    QByteArray getMeId() const;
    QByteArray getCid() const;
    QByteArray getSocId() const;
    bool hasEwcInfo() const;

    QString getKeyDescription(const QString& keyId) const;

private:
    QMap<QString, MtkKeyInfo> keys;


    bool parseMtkKeys(const QJsonObject& json);
    QByteArray decodeHexString(const QString& hexString) const;
    EwcInfo ewcInfo;
    bool ewcLoaded;

    void parseEwcIni(const QString& content);
    QString getPlatform() const;
    QString getFirmwareVersion() const;

    // Методы для извлечения MTK ключей
    void extractMtkKeys(const QVariantMap& data);
};
