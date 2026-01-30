#include "KeyParser.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QByteArray>
#include <QRegularExpression>

bool KeyParser::parseKeysFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open keys file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    // Пробуем парсить как JSON
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qWarning() << "Invalid JSON in keys file";

        // Пробуем парсить как простой текст (ключ=значение)
        QString content = QString::fromUtf8(data);
        QStringList lines = content.split('\n', Qt::SkipEmptyParts);

        QVariantMap keyMap;
        for (const QString& line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith('#')) {
                continue;
            }

            int equalsPos = trimmed.indexOf('=');
            if (equalsPos > 0) {
                QString key = trimmed.left(equalsPos).trimmed();
                QString value = trimmed.mid(equalsPos + 1).trimmed();
                keyMap[key] = value;
            }
        }

        if (!keyMap.isEmpty()) {
            extractMtkKeys(keyMap);
            return !keys.isEmpty();
        }

        return false;
    }

    QJsonObject root = doc.object();

    // Парсим как MTK keys.json
    return parseMtkKeys(root);
}

bool KeyParser::parseMtkKeys(const QJsonObject& json)
{
    QVariantMap data;

    // Конвертируем JSON в QVariantMap
    for (auto it = json.begin(); it != json.end(); ++it) {
        data[it.key()] = it.value().toVariant();
    }

    extractMtkKeys(data);

    qDebug() << "Loaded" << keys.size() << "MTK keys";
    return !keys.isEmpty();
}

void KeyParser::extractMtkKeys(const QVariantMap& data)
{
    keys.clear();

    // Извлекаем MTK ключи с описаниями
    struct KeyDefinition {
        QString keyName;
        QString description;
    };

    static const QVector<KeyDefinition> keyDefinitions = {
        {"MTK_RPMBKEY", "RPMB (Replay Protected Memory Block) Key"},
        {"MTK_RPMB2KEY", "RPMB2 Key"},
        {"MTK_FDEKEY", "Full Disk Encryption Key"},
        {"MTK_ME_ID", "MediaTek ME (Modem) ID"},
        {"MTK_CID", "Chip ID"},
        {"MTK_SOCID", "SoC ID"},
        {"MTK_HRID", "Hardware ID"},
        {"MTK_RID", "Random ID"},
        {"MTK_CHID", "Chip ID (Hex)"},
        {"MTK_ITRUSTEE", "iTrustee Key"},
        {"ChainType", "Chain Type"}
    };

    for (const auto& def : keyDefinitions) {
        if (data.contains(def.keyName)) {
            QString keyValue = data[def.keyName].toString();
            if (!keyValue.isEmpty()) {
                MtkKeyInfo info;
                info.key = keyValue;
                info.description = def.description;
                keys[def.keyName] = info;
                qDebug() << "Loaded key:" << def.keyName << "->" << keyValue.left(16) << "...";
            }
        }
    }

    // Также добавляем все остальные ключи без описаний
    for (auto it = data.begin(); it != data.end(); ++it) {
        QString keyName = it.key();
        if (!keys.contains(keyName)) {
            QString keyValue = it.value().toString();
            if (!keyValue.isEmpty()) {
                MtkKeyInfo info;
                info.key = keyValue;
                info.description = "Unknown MTK key";
                keys[keyName] = info;
                qDebug() << "Loaded unknown key:" << keyName << "->" << keyValue.left(16) << "...";
            }
        }
    }
}

QByteArray KeyParser::getKey(const QString& keyId) const
{
    if (!keys.contains(keyId)) {
        return QByteArray();
    }

    return decodeHexString(keys[keyId].key);
}

bool KeyParser::hasKey(const QString& keyId) const
{
    return keys.contains(keyId);
}

QMap<QString, QString> KeyParser::getAllKeys() const
{
    QMap<QString, QString> result;
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        result[it.key()] = it.value().description;
    }
    return result;
}

QByteArray KeyParser::getRpmbKey() const
{
    return getKey("MTK_RPMBKEY");
}

QByteArray KeyParser::getFdeKey() const
{
    return getKey("MTK_FDEKEY");
}

QByteArray KeyParser::getMeId() const
{
    return getKey("MTK_ME_ID");
}

QByteArray KeyParser::getCid() const
{
    return getKey("MTK_CID");
}

QByteArray KeyParser::getSocId() const
{
    return getKey("MTK_SOCID");
}

QString KeyParser::getKeyDescription(const QString& keyId) const
{
    if (keys.contains(keyId)) {
        return keys[keyId].description;
    }
    return QString();
}

QByteArray KeyParser::decodeHexString(const QString& hexString) const
{
    if (hexString.isEmpty()) {
        return QByteArray();
    }

    // Удаляем пробелы и не-hex символы
    QString cleaned = hexString;
    cleaned.remove(QRegularExpression("[^0-9A-Fa-f]"));

    if (cleaned.isEmpty() || cleaned.length() % 2 != 0) {
        // Если не hex, возвращаем как есть
        return hexString.toUtf8();
    }

    return QByteArray::fromHex(cleaned.toUtf8());
}


bool KeyParser::hasEwcInfo() const {
    return ewcLoaded;
}


//1119


void KeyParser::parseEwcIni(const QString& content) {
    ewcInfo = EwcInfo();

    QTextStream stream(&const_cast<QString&>(content));
    QString currentSection;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        if (line.isEmpty() || line.startsWith(';') || line.startsWith('#')) {
            continue;
        }

        if (line.startsWith('[') && line.endsWith(']')) {
            currentSection = line.mid(1, line.length() - 2);
            continue;
        }

        int equalsPos = line.indexOf('=');
        if (equalsPos == -1) continue;

        QString key = line.left(equalsPos).trimmed();
        QString value = line.mid(equalsPos + 1).trimmed();

        if (currentSection == "BaseInfo") {
            if (key == "ContentType") ewcInfo.contentType = value;
            else if (key == "ExtractionMethod") ewcInfo.extractionMethod = value;
            else if (key == "ExtractionStartUtc") {
                // Формат: 20251204T130202
                if (value.length() >= 13) {
                    QString dateStr = value.left(8); // 20251204
                    QString timeStr = value.mid(9, 6); // 130202
                    QDate date = QDate::fromString(dateStr, "yyyyMMdd");
                    QTime time = QTime::fromString(timeStr, "HHmmss");
                    if (date.isValid() && time.isValid()) {
                        ewcInfo.extractionStart = QDateTime(date, time, Qt::UTC);
                    }
                }
            }
            else if (key == "InternalModelName") ewcInfo.internalModelName = value;
            else if (key == "ProductName") ewcInfo.productName = value;
            else if (key == "ProductVersion") ewcInfo.productVersion = value;
        }
        else if (currentSection == "DeviceInfo") {
            if (key == "DeviceAlias") ewcInfo.deviceAlias = value;
        }
        else if (currentSection == "ExtendedInfo") {
            if (key == "KeyBagFile") ewcInfo.keyBagFile = value;
            else if (key.startsWith("Partition ")) {
                // Пример: "Partition 1 Name=userdata"
                QRegularExpression re("Partition (\\d+) (\\w+)");
                QRegularExpressionMatch match = re.match(key);
                if (match.hasMatch()) {
                    int index = match.captured(1).toInt() - 1;
                    QString field = match.captured(2);

                    // Увеличиваем список если нужно
                    while (ewcInfo.partitions.size() <= index) {
                        ewcInfo.partitions.append(EwcInfo::PartitionInfo());
                    }

                    EwcInfo::PartitionInfo& part = ewcInfo.partitions[index];

                    if (field == "Name") part.name = value;
                    else if (field == "File") part.file = value;
                    else if (field == "Size") part.size = value.toLongLong();
                    else if (field == "MD5") part.md5 = value;
                    else if (field == "SHA1") part.sha1 = value;
                    else if (field == "SHA256") part.sha256 = value;
                    else if (field == "SHA3v256") part.sha3256 = value;
                }
            }
            else if (key == "PartitionsCount") {
                // Уже обрабатывается через Partition X entries
            }
        }
    }

    // Определяем платформу по модели
    QString model = ewcInfo.internalModelName.toUpper();
    if (model.contains("MT")) ewcInfo.deviceAlias = "MTK";
    else if (model.contains("SPD")) ewcInfo.deviceAlias = "SPD";
    else if (model.contains("EXN") || model.contains("EXYNOS")) ewcInfo.deviceAlias = "EXN";
    else if (model.contains("QCM") || model.contains("SNAPDRAGON")) ewcInfo.deviceAlias = "QCM";
    else if (model.contains("KIRIN")) ewcInfo.deviceAlias = "KRN";

    qDebug() << "Parsed EWC file with" << ewcInfo.partitions.size() << "partitions";
}

QString KeyParser::getPlatform() const {
    return ewcInfo.deviceAlias;
}

QString KeyParser::getFirmwareVersion() const {
    // TODO: Извлечь из дополнительных полей
    return ewcInfo.productVersion;
}
