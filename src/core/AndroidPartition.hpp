#pragma once

#include <QString>
#include <QDateTime>

struct AndroidPartition
{
    QString name;
    QString type;
    qint64 offset;
    qint64 size;
    QString filePath;
    bool isEncrypted;
    QString encryptionType;
    QString keyId;

    bool hasRpmbKey;
    bool hasFdeKey;
    bool hasMeId;

    QDateTime modified;

    AndroidPartition() :
        offset(0),
        size(0),
        isEncrypted(false),
        hasRpmbKey(false),
        hasFdeKey(false),
        hasMeId(false)
    {}

    bool isValid() const { return size > 0 && !name.isEmpty(); }
    QString humanReadableSize() const;

    bool canBeDecrypted() const {
        return isEncrypted && (hasFdeKey || hasRpmbKey);
    }

    // Добавляем оператор сравнения
    bool operator==(const AndroidPartition& other) const {
        return name == other.name &&
               offset == other.offset &&
               size == other.size &&
               filePath == other.filePath;
    }
};
