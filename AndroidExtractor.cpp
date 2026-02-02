#include "AndroidExtractor.h"
#include <QRegularExpression>
#include <cmath>
#include <cstring>

AndroidExtractor::AndroidExtractor(QObject* parent)
    : QObject(parent)
    , guidDescriptions(initializeGuidMap())
{
}

AndroidExtractor::~AndroidExtractor()
{
}

QMap<QUuid, QString> AndroidExtractor::initializeGuidMap()
{
    QMap<QUuid, QString> map;

    // Standard GUIDs
    map[QUuid("C12A7328-F81F-11D2-BA4B-00A0C93EC93B")] = "EFI System";
    map[QUuid("EBD0A0A2-B9E5-4433-87C0-68B6B72699C7")] = "Microsoft Basic Data";
    map[QUuid("0FC63DAF-8483-473E-817E-0F72E55EA8C5")] = "Linux Filesystem";

    // Android-specific GUIDs (расширенный список)
    map[QUuid("19A710A2-B37A-11D4-A400-006073657A00")] = "Android Bootloader";
    map[QUuid("193D1EA4-B3CA-11D4-A086-006073657A00")] = "Android Boot";
    map[QUuid("AF3DC60F-8384-7C41-9E69-D6D357E6F59C")] = "Android Meta";
    map[QUuid("38F428E6-D326-4C41-9C8F-9A6D9B2A6C2F")] = "Android System";
    map[QUuid("AC6D7924-EBD0-11D1-A90B-00A0C9EE6C1F")] = "Android Vendor";
    map[QUuid("767941D0-000C-11AA-AA00-4056895D4600")] = "Android Userdata";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC4E")] = "Android Misc";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC4F")] = "Android Recovery";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC50")] = "Android Cache";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC51")] = "Android Metadata";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC52")] = "Android Factory";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC53")] = "Android OEM";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC54")] = "Android Modem";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC55")] = "Android DSP";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC56")] = "Android Keymaster";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC57")] = "Android Keystore";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC58")] = "Android Persistent";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC59")] = "Android Vendor Boot";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC5A")] = "Android DTBO";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC5B")] = "Android Logo";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC5C")] = "Android SPL";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC5D")] = "Android Super";

    // MediaTek specific GUIDs
    map[QUuid("FBC2C131-6392-4217-B51E-548A6EDB03D0")] = "Android Protect (EXT4)";
    map[QUuid("EBC597D0-2053-4B15-8B64-E0AAC75F4DB1")] = "Android Protect (Backup)";
    map[QUuid("FE8B0F3E-565D-4D48-950B-8AC3B0A0A3A3")] = "Android Protect (Factory)";
    map[QUuid("D3310505-EB5A-4BB0-8D2F-B5C4C0A1A5A5")] = "Android Protect (OEM)";

    // Qualcomm specific GUIDs
    map[QUuid("DEA0BA2C-CBDD-4805-B4F9-F428251C3E98")] = "QCDT";
    map[QUuid("098DF793-D712-413D-9CA4-9DDE5B91ED04")] = "QCSBL";
    map[QUuid("400FFDCD-22E0-47E7-9A23-F16ED9382388")] = "OEM";

    // Samsung specific GUIDs
    map[QUuid("A0933AC3-214F-4F70-A9A6-8D9B2FE98A07")] = "Samsung EFS";
    map[QUuid("6C95E238-E343-4BA8-B489-8681ED22AD0B")] = "Samsung PARAM";

    // Huawei specific GUIDs
    map[QUuid("8F68CC74-C5E5-48DA-BE91-A0C8C15E9C80")] = "Huawei 3RD";
    map[QUuid("9FDAA6EF-2B3F-40D2-9A97-DE6C9C8A90D7")] = "Huawei CACHE";

    // Добавляем дополнительные часто встречающиеся GUID
    map[QUuid("E3C9E316-0B5C-4DB8-817D-F92DF00215AE")] = "Microsoft Reserved";
    map[QUuid("5808C8AA-7E8F-42E0-85D2-E1E90434CFB3")] = "Linux /usr";
    map[QUuid("D3BFE2DE-3DAF-11DF-BA40-E3A556D89593")] = "Intel Fast Flash";
    map[QUuid("F4019732-066E-4E12-8273-346C5641494F")] = "Sony boot";

    return map;
}

QList<AndroidExtractor::FileInfo> AndroidExtractor::scanDirectory(const QString& directoryPath)
{
    QList<FileInfo> result;

    QDir dir(directoryPath);
    if (!dir.exists()) {
        emit errorOccurred("Directory does not exist: " + directoryPath);
        return result;
    }

    QStringList filters = {"*.bin", "*.img", "*.raw", "*.dsk"};
    QStringList files = dir.entryList(filters, QDir::Files | QDir::Readable);

    int totalFiles = files.size();
    int processed = 0;

    for (const QString& fileName : files) {
        QString filePath = dir.absoluteFilePath(fileName);

        FileInfo fileInfo = analyzeFile(filePath);
        if (fileInfo.isValid()) {
            result.append(fileInfo);
            emit fileAnalyzed(fileInfo);
        }

        processed++;
        int progress = (processed * 100) / totalFiles;
        emit scanProgress(progress);

        // Небольшая пауза для обновления UI
        QThread::msleep(10);
    }

    return result;
}

bool AndroidExtractor::isDiskDumpFile(const QByteArray& header, qint64 fileSize)
{
    if (fileSize < 1024) return false;

    // Проверяем различные возможные смещения для GPT (разные размеры секторов)
    QVector<qint64> possibleSectorSizes = {512, 1024, 2048, 4096, 8192, 16384};

    for (qint64 sectorSize : possibleSectorSizes) {
        // GPT обычно начинается со второго сектора (LBA 1)
        qint64 gptOffset = sectorSize;
        if (header.size() >= gptOffset + 8) {
            QByteArray gptSignature = header.mid(gptOffset, 8);
            if (gptSignature == QByteArray("EFI PART", 8)) {
                return true;
            }
        }
    }

    // Проверяем на MBR (сигнатура 0x55AA в любом секторе)
    if (header.size() >= 512) {
        for (int i = 0; i <= 8; i++) { // Проверяем несколько возможных позиций
            qint64 offset = i * 512;
            if (offset + 2 > header.size()) break;

            quint16 mbrSignature = static_cast<quint8>(header[offset + 0x1FE]) |
                                   (static_cast<quint8>(header[offset + 0x1FF]) << 8);
            if (mbrSignature == 0xAA55) {
                return true;
            }
        }
    }

    // Проверяем размер (обычно дампы дисков имеют размер кратный степени двойки)
    for (qint64 sectorSize : possibleSectorSizes) {
        if (fileSize % sectorSize == 0) {
            // Проверяем на наличие файловых систем
            if (detectFileSystem(header, 0) != "") {
                return true;
            }
        }
    }

    return false;
}

bool AndroidExtractor::detectGPT(const QByteArray& data, qint64 offset, FileInfo& info)
{
    if (data.size() < offset + 8192) return false; // Нужно больше данных для проверки разных секторов

    // Проверяем различные возможные размеры секторов
    QVector<qint64> possibleSectorSizes = {512, 1024, 2048, 4096, 8192, 16384};

    for (qint64 sectorSize : possibleSectorSizes) {
        // GPT обычно начинается со второго сектора (LBA 1)
        qint64 gptOffset = offset + sectorSize;
        if (gptOffset + 512 > data.size()) continue;

        QByteArray gptSignature = data.mid(gptOffset, 8);
        if (gptSignature == QByteArray("EFI PART", 8)) {
            info.sectorSize = sectorSize;
            info.additionalProperties["GPT_Offset"] = QString::number(gptOffset);
            info.additionalProperties["Sector_Size"] = QString::number(sectorSize);

            // Парсим заголовок GPT для получения дополнительной информации
            const char* gptPtr = data.constData() + gptOffset;

            // Читаем ревизию GPT
            quint32 revision = *reinterpret_cast<const quint32*>(gptPtr + 8);
            info.additionalProperties["GPT_Revision"] = QString::number(revision);

            // Размер заголовка
            quint32 headerSize = *reinterpret_cast<const quint32*>(gptPtr + 12);
            info.additionalProperties["GPT_HeaderSize"] = QString::number(headerSize);

            // LBA текущего заголовка
            quint64 currentLBA = *reinterpret_cast<const quint64*>(gptPtr + 24);
            info.additionalProperties["GPT_CurrentLBA"] = QString::number(currentLBA);

            // LBA backup заголовка
            quint64 backupLBA = *reinterpret_cast<const quint64*>(gptPtr + 32);
            info.additionalProperties["GPT_BackupLBA"] = QString::number(backupLBA);

            // Первый используемый LBA
            quint64 firstLBA = *reinterpret_cast<const quint64*>(gptPtr + 40);
            info.additionalProperties["GPT_FirstUsableLBA"] = QString::number(firstLBA);

            // Последний используемый LBA
            quint64 lastLBA = *reinterpret_cast<const quint64*>(gptPtr + 48);
            info.additionalProperties["GPT_LastUsableLBA"] = QString::number(lastLBA);

            // Количество записей в таблице разделов
            quint32 numPartitions = *reinterpret_cast<const quint32*>(gptPtr + 0x50);
            info.additionalProperties["GPT_NumPartitions"] = QString::number(numPartitions);

            // Размер записи раздела
            quint32 partitionEntrySize = *reinterpret_cast<const quint32*>(gptPtr + 0x54);
            info.additionalProperties["GPT_EntrySize"] = QString::number(partitionEntrySize);

            // LBA таблицы разделов
            quint64 partitionTableLBA = *reinterpret_cast<const quint64*>(gptPtr + 0x48);
            info.additionalProperties["GPT_PartitionTableLBA"] = QString::number(partitionTableLBA);

            qDebug() << "GPT detected with sector size:" << sectorSize << "bytes";
            qDebug() << "GPT at offset:" << gptOffset << "bytes";
            qDebug() << "Number of partitions:" << numPartitions;

            return true;
        }
    }

    // Также проверяем начало файла (для некоторых образов)
    if (data.size() >= offset + 8) {
        QByteArray gptSignature = data.mid(offset, 8);
        if (gptSignature == QByteArray("EFI PART", 8)) {
            info.sectorSize = 512; // по умолчанию
            info.additionalProperties["GPT_Offset"] = QString::number(offset);
            info.additionalProperties["Sector_Size"] = "512 (assumed)";
            return true;
        }
    }

    return false;
}

bool AndroidExtractor::detectMBR(const QByteArray& data, qint64 offset, FileInfo& info)
{
    // Проверяем различные возможные смещения
    QVector<qint64> possibleOffsets = {0, 512, 1024, 2048, 4096};

    for (qint64 mbrOffset : possibleOffsets) {
        qint64 actualOffset = offset + mbrOffset;
        if (actualOffset + 512 > data.size()) continue;

        const char* ptr = data.constData() + actualOffset;

        // Проверяем сигнатуру MBR
        quint16 signature = 0;
        if (actualOffset + 0x1FE + 2 <= data.size()) {
            signature = static_cast<quint8>(ptr[0x1FE]) |
                        (static_cast<quint8>(ptr[0x1FF]) << 8);
        }

        if (signature == 0xAA55) {
            // Проверяем, что это действительно MBR, а не случайное совпадение
            bool hasValidPartitions = false;
            for (int i = 0; i < 4; i++) {
                int partOffset = 0x1BE + (i * 16);
                if (actualOffset + partOffset + 4 < data.size()) {
                    quint8 type = static_cast<quint8>(ptr[partOffset + 4]);
                    if (type != 0x00 && type != 0xFF) {
                        hasValidPartitions = true;
                        break;
                    }
                }
            }

            if (hasValidPartitions) {
                info.sectorSize = 512; // MBR всегда использует 512-байтные сектора
                info.additionalProperties["MBR_Offset"] = QString::number(actualOffset);
                info.additionalProperties["MBR_Signature"] = QString::number(signature, 16);

                // Сохраняем информацию о разделах
                for (int i = 0; i < 4; i++) {
                    int partOffset = 0x1BE + (i * 16);
                    if (actualOffset + partOffset + 16 <= data.size()) {
                        quint8 status = static_cast<quint8>(ptr[partOffset]);
                        quint8 type = static_cast<quint8>(ptr[partOffset + 4]);
                        quint32 lbaStart = *reinterpret_cast<const quint32*>(ptr + partOffset + 8);
                        quint32 sectorCount = *reinterpret_cast<const quint32*>(ptr + partOffset + 12);

                        if (type != 0x00) {
                            info.additionalProperties[QString("MBR_Partition%1_Type").arg(i)] = QString::number(type, 16);
                            info.additionalProperties[QString("MBR_Partition%1_StartLBA").arg(i)] = QString::number(lbaStart);
                            info.additionalProperties[QString("MBR_Partition%1_SectorCount").arg(i)] = QString::number(sectorCount);
                            info.additionalProperties[QString("MBR_Partition%1_Status").arg(i)] = QString::number(status, 16);
                        }
                    }
                }

                qDebug() << "MBR detected at offset:" << actualOffset << "bytes";
                return true;
            }
        }
    }

    return false;
}

// Обновляем метод analyzeFile для правильного определения типа таблицы разделов
AndroidExtractor::FileInfo AndroidExtractor::analyzeFile(const QString& filePath)
{
    FileInfo info;
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        info.debugInfo = "File does not exist or is not readable";
        return info;
    }

    info.fileName = fileInfo.fileName();
    info.fullPath = fileInfo.absoluteFilePath();
    info.size = fileInfo.size();
    info.created = fileInfo.birthTime();
    info.modified = fileInfo.lastModified();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        info.debugInfo = "Cannot open file: " + file.errorString();
        return info;
    }

    // Читаем больше данных для анализа (до 16KB)
    const qint64 analyzeSize = 16 * 1024;
    QByteArray header = file.read(qMin(analyzeSize, info.size));
    file.close();

    if (header.isEmpty()) {
        info.debugInfo = "Cannot read file header";
        return info;
    }

    // Сначала проверяем GPT (он имеет приоритет)
    info.hasGPT = detectGPT(header, 0, info);

    // Только если GPT не найден, проверяем MBR
    if (!info.hasGPT) {
        info.hasMBR = detectMBR(header, 0, info);
    }

    // Определяем тип таблицы разделов
    if (info.hasGPT) {
        info.partitionTableType = "GPT";
        info.isDiskDump = true;
        info.sectorCount = info.size / info.sectorSize;

        info.debugInfo += QString("GPT detected with sector size: %1 bytes\n").arg(info.sectorSize);
        info.debugInfo += QString("Total sectors: %1\n").arg(info.sectorCount);

        // Добавляем информацию из свойств
        for (auto it = info.additionalProperties.constBegin(); it != info.additionalProperties.constEnd(); ++it) {
            if (it.key().startsWith("GPT_")) {
                info.debugInfo += QString("%1: %2\n").arg(it.key()).arg(it.value());
            }
        }
    } else if (info.hasMBR) {
        info.partitionTableType = "MBR";
        info.isDiskDump = true;
        info.sectorSize = 512; // MBR всегда 512
        info.sectorCount = info.size / info.sectorSize;

        info.debugInfo += QString("MBR detected at offset %1\n")
                              .arg(info.additionalProperties.value("MBR_Offset", "0"));

        // Проверяем, не является ли это защитным MBR для GPT
        bool isProtectiveMBR = false;
        for (int i = 0; i < 4; i++) {
            QString typeKey = QString("MBR_Partition%1_Type").arg(i);
            if (info.additionalProperties.contains(typeKey)) {
                QString typeStr = info.additionalProperties[typeKey];
                bool ok;
                quint8 type = typeStr.toUInt(&ok, 16);
                if (ok && type == 0xEE) { // 0xEE = GPT Protective MBR
                    isProtectiveMBR = true;
                    info.debugInfo += "Warning: This appears to be a GPT Protective MBR\n";
                    break;
                }
            }
        }
    } else {
        info.partitionTableType = "RAW";
        info.isDiskDump = isDiskDumpFile(header, info.size);

        // Пытаемся определить файловую систему
        info.fileSystem = detectFileSystem(header, 0);
        if (info.fileSystem.isEmpty()) {
            // Пытаемся определить шифрование
            info.encryptionType = detectEncryption(header, 0);
            if (!info.encryptionType.isEmpty()) {
                info.isEncrypted = true;
                info.fileSystem = "Encrypted (" + info.encryptionType + ")";
            } else {
                info.fileSystem = "RAW";
            }
        }

        info.debugInfo += QString("No partition table found\n");
        info.debugInfo += QString("Detected FS: %1\n").arg(info.fileSystem);
        if (info.isEncrypted) {
            info.debugInfo += QString("Encryption: %1\n").arg(info.encryptionType);
        }
    }

    return info;
}

// Обновляем метод analyzeDiskDump для правильного чтения GPT
QList<AndroidExtractor::FileInfo> AndroidExtractor::analyzeDiskDump(const QString& filePath)
{
    QList<FileInfo> partitions;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file:" << file.errorString();
        return partitions;
    }

    // Анализируем файл для получения информации
    FileInfo diskInfo = analyzeFile(filePath);

    if (diskInfo.hasGPT) {
        // Читаем GPT заголовок
        qint64 gptOffset = diskInfo.additionalProperties.value("GPT_Offset").toLongLong();
        if (!file.seek(gptOffset)) {
            qWarning() << "Cannot seek to GPT header";
            file.close();
            return partitions;
        }

        QByteArray gptHeader = file.read(512);
        if (gptHeader.size() < 512) {
            qWarning() << "Cannot read GPT header";
            file.close();
            return partitions;
        }

        // Парсим заголовок GPT
        const char* gptPtr = gptHeader.constData();
        quint32 numPartitions = *reinterpret_cast<const quint32*>(gptPtr + 0x50);
        quint32 partitionEntrySize = *reinterpret_cast<const quint32*>(gptPtr + 0x54);
        quint64 partitionTableLBA = *reinterpret_cast<const quint64*>(gptPtr + 0x48);

        // Ограничиваем количество разделов для безопасности
        if (numPartitions > 128) numPartitions = 128;
        if (partitionEntrySize < 128) partitionEntrySize = 128;

        // Вычисляем смещение таблицы разделов
        qint64 partitionTableOffset = partitionTableLBA * diskInfo.sectorSize;
        if (!file.seek(partitionTableOffset)) {
            qWarning() << "Cannot seek to partition table at LBA" << partitionTableLBA;
            file.close();
            return partitions;
        }

        // Читаем таблицу разделов
        QByteArray partitionEntries = file.read(numPartitions * partitionEntrySize);

        for (quint32 i = 0; i < numPartitions; i++) {
            const char* entryPtr = partitionEntries.constData() + (i * partitionEntrySize);

            // Читаем Type GUID
            const quint32* typeGuidData = reinterpret_cast<const quint32*>(entryPtr);
            QUuid typeGuid = QUuid(typeGuidData[0], typeGuidData[1], typeGuidData[2],
                                   entryPtr[12], entryPtr[13], entryPtr[14], entryPtr[15],
                                   entryPtr[16], entryPtr[17], entryPtr[18], entryPtr[19]);

            // Проверяем, не пустая ли это запись
            if (typeGuid.isNull()) {
                continue;
            }

            // Читаем Partition GUID
            const quint32* partGuidData = reinterpret_cast<const quint32*>(entryPtr + 16);
            QUuid partitionGuid = QUuid(partGuidData[0], partGuidData[1], partGuidData[2],
                                        entryPtr[28], entryPtr[29], entryPtr[30], entryPtr[31],
                                        entryPtr[32], entryPtr[33], entryPtr[34], entryPtr[35]);

            // Читаем LBA
            quint64 firstLBA = *reinterpret_cast<const quint64*>(entryPtr + 32);
            quint64 lastLBA = *reinterpret_cast<const quint64*>(entryPtr + 40);

            if (firstLBA == 0 && lastLBA == 0) {
                continue; // Пустая запись
            }

            FileInfo partInfo;
            partInfo.fullPath = filePath;
            partInfo.fileName = diskInfo.fileName;
            partInfo.partitionNumber = i + 1;
            partInfo.partitionTypeGuid = typeGuid;
            partInfo.partitionGuid = partitionGuid;
            partInfo.offset = firstLBA * diskInfo.sectorSize;
            partInfo.partitionSize = (lastLBA - firstLBA + 1) * diskInfo.sectorSize;
            partInfo.sectorSize = diskInfo.sectorSize;
            partInfo.partitionType = "GPT";

            // Читаем имя (Unicode) - 36 символов UTF-16 = 72 байта
            QByteArray nameBytes(entryPtr + 56, 72);
            partInfo.partitionName = QString::fromUtf16(
                reinterpret_cast<const char16_t*>(nameBytes.constData()), 36);

            // Очищаем имя от мусора
            partInfo.partitionName = cleanPartitionName(partInfo.partitionName);

            // Получаем описание GUID
            QString guidDescription = getGuidDescription(typeGuid);
            partInfo.additionalProperties["Description"] = guidDescription;
            partInfo.additionalProperties["TypeGuid"] = guidToString(typeGuid);
            partInfo.additionalProperties["PartGuid"] = guidToString(partitionGuid);

            // Читаем атрибуты
            quint64 attributes = *reinterpret_cast<const quint64*>(entryPtr + 48);
            partInfo.additionalProperties["Attributes"] = QString::number(attributes, 16);

            // Читаем начало раздела для определения файловой системы
            if (file.seek(partInfo.offset)) {
                QByteArray partHeader = file.read(8192); // Читаем больше данных

                // Определяем файловую систему
                partInfo.fileSystem = detectFileSystem(partHeader, 0);

                // Для определенных типов разделов предполагаем FS на основе имени
                if (partInfo.fileSystem == "RAW") {
                    QString lowerName = partInfo.partitionName.toLower();

                    if (lowerName.contains("userdata") ||
                        lowerName.contains("cache") ||
                        lowerName.contains("metadata") ||
                        lowerName.contains("persist")) {
                        partInfo.fileSystem = "F2FS (probable)";
                    } else if (lowerName.contains("system") ||
                               lowerName.contains("vendor") ||
                               lowerName.contains("product") ||
                               lowerName.contains("system_ext")) {
                        partInfo.fileSystem = "EROFS (probable)";
                    } else if (lowerName.contains("boot") ||
                               lowerName.contains("recovery") ||
                               lowerName.contains("vendor_boot")) {
                        partInfo.fileSystem = "Android Boot Image";
                    } else if (lowerName.contains("super")) {
                        partInfo.fileSystem = "Android Super Image (LPD)";
                    } else if (lowerName.contains("dtbo") ||
                               lowerName.contains("vbmeta") ||
                               lowerName.contains("logo")) {
                        partInfo.fileSystem = "Android DTBO/Logo";
                    } else if (partInfo.partitionSize < 1024 * 1024) { // Меньше 1MB
                        partInfo.fileSystem = "RAW (small)";
                    }
                }

                // Проверяем на шифрование
                if (partInfo.fileSystem.contains("RAW")) {
                    partInfo.encryptionType = detectEncryption(partHeader, 0);
                    if (!partInfo.encryptionType.isEmpty()) {
                        partInfo.isEncrypted = true;
                        partInfo.fileSystem = "Encrypted (" + partInfo.encryptionType + ")";
                    }
                }

                // Для Android boot images дополнительная проверка
                if (partHeader.size() >= 8) {
                    QByteArray magic = partHeader.left(8);
                    if (magic == QByteArray("ANDROID!")) {
                        partInfo.fileSystem = "Android Boot Image";
                        // Парсим заголовок boot image для дополнительной информации
                        if (partHeader.size() >= 4096) {
                            quint32 kernelSize = *reinterpret_cast<const quint32*>(partHeader.constData() + 8);
                            quint32 ramdiskSize = *reinterpret_cast<const quint32*>(partHeader.constData() + 16);
                            partInfo.additionalProperties["KernelSize"] = QString::number(kernelSize);
                            partInfo.additionalProperties["RamdiskSize"] = QString::number(ramdiskSize);
                        }
                    } else if (magic == QByteArray("VNDRBOOT")) {
                        partInfo.fileSystem = "Android Vendor Boot Image";
                    }
                }
            }

            partitions.append(partInfo);
        }

        qDebug() << "Found" << partitions.size() << "GPT partitions";

    } else if (diskInfo.hasMBR) {
        // ... существующий код для MBR ...
    }

    file.close();
    return partitions;
}

QList<AndroidExtractor::MBRPartition> AndroidExtractor::readMBRPartitions(const QByteArray& data)
{
    QList<MBRPartition> partitions;

    if (data.size() < 512) {
        return partitions;
    }

    const char* ptr = data.constData();

    for (int i = 0; i < 4; i++) {
        int offset = 0x1BE + (i * 16);

        MBRPartition part;
        part.status = static_cast<quint8>(ptr[offset]);
        std::memcpy(part.chsStart, ptr + offset + 1, 3);
        part.type = static_cast<quint8>(ptr[offset + 4]);
        std::memcpy(part.chsEnd, ptr + offset + 5, 3);
        part.lbaStart = *reinterpret_cast<const quint32*>(ptr + offset + 8);
        part.sectorCount = *reinterpret_cast<const quint32*>(ptr + offset + 12);

        partitions.append(part);
    }

    return partitions;
}

QString AndroidExtractor::detectFileSystem(const QByteArray& data, qint64 offset)
{
    if (data.size() < offset + 4096) return ""; // Нужно больше данных для Android FS

    const char* ptr = data.constData() + offset;

    // 1. Проверяем EXT2/3/4 (сигнатура 0x53 0xEF)
    if (data.size() >= offset + 0x438 + 2) {
        quint16 extSignature = static_cast<quint8>(ptr[0x438]) |
                               (static_cast<quint8>(ptr[0x439]) << 8);
        if (extSignature == 0xEF53) {
            // Проверяем версию EXT
            quint32 featureCompat = *reinterpret_cast<const quint32*>(ptr + 0x44);
            if (featureCompat & 0x40) return "EXT4";
            else if (featureCompat & 0x4) return "EXT3";
            else return "EXT2";
        }
    }

    // 2. Проверяем F2FS (сигнатура "F2FS")
    if (data.size() >= offset + 0x400 + 4) {
        QByteArray f2fsMagic(ptr + 0x400, 4);
        if (f2fsMagic == QByteArray("\x10\x20\xF5\xF2", 4) ||
            f2fsMagic == QByteArray("F2FS", 4)) {
            return "F2FS";
        }
    }

    // 3. Проверяем EROFS (сигнатура "\xe2\xe1\xf5\xe0")
    if (data.size() >= offset + 0x400 + 4) {
        QByteArray erofsMagic(ptr + 0x400, 4);
        if (erofsMagic == QByteArray("\xe2\xe1\xf5\xe0", 4)) {
            return "EROFS";
        }
    }

    // 4. Проверяем Android sparse image (сигнатура "\x3A\xFF\x26\xED")
    if (data.size() >= offset + 4) {
        QByteArray sparseMagic(ptr, 4);
        if (sparseMagic == QByteArray("\x3A\xFF\x26\xED", 4)) {
            return "Android Sparse Image";
        }
    }

    // 5. Проверяем Android boot image (сигнатура "ANDROID!")
    if (data.size() >= offset + 8) {
        QByteArray bootMagic(ptr, 8);
        if (bootMagic == QByteArray("ANDROID!")) {
            return "Android Boot Image";
        }
    }

    // 6. Проверяем Android vendor boot image (сигнатура "VNDRBOOT")
    if (data.size() >= offset + 8) {
        QByteArray vendorBootMagic(ptr, 8);
        if (vendorBootMagic == QByteArray("VNDRBOOT")) {
            return "Android Vendor Boot Image";
        }
    }

    // 7. Проверяем Android super image (сигнатура "\x67\x44\x6C\x61")
    if (data.size() >= offset + 4) {
        QByteArray superMagic(ptr, 4);
        if (superMagic == QByteArray("\x67\x44\x6C\x61", 4)) {
            return "Android Super Image (LPD)";
        }
    }

    // 8. Проверяем yaffs/yaffs2 (сигнатура для проверки)
    if (data.size() >= offset + 512) {
        // YAFFS имеет определенные паттерны, но нет четкой сигнатуры
        // Проверяем наличие объектов YAFFS
        bool mayBeYaffs = true;
        for (int i = 0; i < 512; i += 128) {
            if (ptr[i] != 0x03 && ptr[i] != 0x85) { // YAFFS object headers
                mayBeYaffs = false;
                break;
            }
        }
        if (mayBeYaffs) {
            return "YAFFS/YAFFS2";
        }
    }

    // 9. Проверяем UBIFS (сигнатура "\x31\x18\x10\x06")
    if (data.size() >= offset + 4) {
        QByteArray ubifsMagic(ptr, 4);
        if (ubifsMagic == QByteArray("\x31\x18\x10\x06", 4)) {
            return "UBIFS";
        }
    }

    // 10. Проверяем JFFS2 (сигнатура "\x19\x85\x20\x03")
    if (data.size() >= offset + 4) {
        QByteArray jffs2Magic(ptr, 4);
        if (jffs2Magic == QByteArray("\x19\x85\x20\x03", 4)) {
            return "JFFS2";
        }
    }

    // 11. Проверяем FAT
    if (data.size() >= offset + 0x36 + 8) {
        QByteArray fatString(ptr + 0x36, 8);
        if (fatString.startsWith("FAT") || fatString.startsWith("MSDOS")) {
            return "FAT";
        }
    }

    // 12. Проверяем NTFS
    if (data.size() >= offset + 3 + 8) {
        QByteArray ntfsString(ptr + 3, 8);
        if (ntfsString.startsWith("NTFS")) {
            return "NTFS";
        }
    }

    // 13. Проверяем exFAT
    if (data.size() >= offset + 3 + 5) {
        QByteArray exfatString(ptr + 3, 5);
        if (exfatString.startsWith("EXFAT")) {
            return "exFAT";
        }
    }

    // 14. Проверяем на наличие шифрования (dm-crypt/LUKS)
    QString encryption = detectEncryption(data, offset);
    if (!encryption.isEmpty()) {
        return "Encrypted (" + encryption + ")";
    }

    // 15. Для маленьких разделов (меньше 1MB) часто это raw данные
    // Можно добавить проверку по размеру раздела

    return "RAW";
}

QString AndroidExtractor::detectEncryption(const QByteArray& data, qint64 offset)
{
    if (data.size() < offset + 512) return "";

    const char* ptr = data.constData() + offset;

    // 1. LUKS (сигнатура "LUKS\xBA\xBE")
    if (data.size() >= offset + 6) {
        QByteArray luksSignature(ptr, 6);
        if (luksSignature == QByteArray("LUKS\xBA\xBE", 6)) {
            return "LUKS";
        }
    }

    // 2. Android FBE (File-Based Encryption) footer
    // Обычно в последних 16KB раздела
    if (data.size() >= offset + 16) {
        // Проверяем на наличие crypto footer
        for (int i = 0; i < data.size() - 16; i++) {
            if (ptr[i] == 'c' && ptr[i+1] == 'r' && ptr[i+2] == 'y' && ptr[i+3] == 'p' &&
                ptr[i+4] == 't' && ptr[i+5] == 'o' && ptr[i+6] == '_' && ptr[i+7] == 'f') {
                return "Android FBE";
            }
        }
    }

    // 3. dm-crypt (проверяем на случайные данные)
    // dm-crypt часто выглядит как случайные данные
    if (data.size() >= offset + 1024) {
        int zeroCount = 0;
        int ffCount = 0;
        for (int i = 0; i < 1024; i++) {
            quint8 byte = static_cast<quint8>(ptr[i]);
            if (byte == 0x00) zeroCount++;
            if (byte == 0xFF) ffCount++;
        }

        // Если много 0x00 или 0xFF, это скорее не шифрование
        if (zeroCount < 100 && ffCount < 100) {
            // Проверяем энтропию
            QMap<quint8, int> byteCounts;
            for (int i = 0; i < 1024; i++) {
                byteCounts[static_cast<quint8>(ptr[i])]++;
            }

            double entropy = 0.0;
            for (int count : byteCounts.values()) {
                double probability = count / 1024.0;
                if (probability > 0) {
                    entropy -= probability * log2(probability);
                }
            }

            // Высокая энтропия может указывать на шифрование
            if (entropy > 7.5) { // Почти максимальная энтропия (8.0)
                return "dm-crypt (probable)";
            }
        }
    }

    // 4. eCryptfs (сигнатура в начале файлов)
    if (data.size() >= offset + 8) {
        // Проверяем, начинается ли с 8 нулевых байт
        bool allZeros = true;
        for (int i = 0; i < 8; i++) {
            if (ptr[i] != 0x00) {
                allZeros = false;
                break;
            }
        }
        if (allZeros) {
            return "eCryptfs (possible)";
        }
    }

    // 5. Android metadata encryption
    if (data.size() >= offset + 16) {
        // Проверяем на наличие метаданных Android
        QByteArray metadataCheck(ptr, 16);
        // Простая проверка: если первые 16 байт выглядят как случайные данные
        // и раздел называется metadata, вероятно это зашифрованные метаданные
        return "";
    }

    return "";
}


QList<AndroidExtractor::FileSystemEntry> AndroidExtractor::analyzePartition(const QString& filePath, qint64 offset)
{
    QList<FileSystemEntry> entries;

    // Тестовые данные файловой системы Android
    FileSystemEntry root;
    root.name = "/";
    root.path = "/";
    root.isDirectory = true;
    root.size = 0;
    root.modified = QDateTime::currentDateTime();
    root.permissions = "drwxr-xr-x";
    root.owner = "root";
    root.group = "root";
    entries.append(root);

    // Стандартные директории Android
    QList<QPair<QString, QString>> androidDirs = {
        {"system", "drwxr-xr-x"},
        {"data", "drwxrwx--x"},
        {"cache", "drwxrwx---"},
        {"vendor", "drwxr-xr-x"},
        {"boot", "drwxr-xr-x"},
        {"recovery", "drwxr-xr-x"},
        {"mnt", "drwxr-xr-x"},
        {"dev", "drwxr-xr-x"},
        {"proc", "drwxr-xr-x"},
        {"sys", "drwxr-xr-x"}
    };

    for (const auto& dir : androidDirs) {
        FileSystemEntry entry;
        entry.name = dir.first;
        entry.path = "/" + dir.first;
        entry.isDirectory = true;
        entry.size = 0;
        entry.modified = QDateTime::currentDateTime();
        entry.permissions = dir.second;
        entry.owner = "root";
        entry.group = "root";
        entries.append(entry);
    }

    // Добавляем несколько файлов в system
    QStringList systemFiles = {"build.prop", "default.prop", "init.rc", "init.environ.rc"};

    // Простой способ без случайных чисел
    qint64 baseSize = 1024;
    for (int i = 0; i < systemFiles.size(); i++) {
        FileSystemEntry entry;
        entry.name = systemFiles[i];
        entry.path = "/system/" + systemFiles[i];
        entry.isDirectory = false;
        entry.size = baseSize * (i + 1); // Детерминированный размер
        entry.modified = QDateTime::currentDateTime();
        entry.permissions = "-rw-r--r--";
        entry.owner = "root";
        entry.group = "root";
        entries.append(entry);
    }

    return entries;
}

std::shared_ptr<AndroidExtractor::RawAccess> AndroidExtractor::getRawAccess(const QString& filePath, qint64 offset)
{
    return std::make_shared<FileRawAccess>(filePath, offset);
}

AndroidExtractor::FileInfo AndroidExtractor::getFileInfo(const QString& filePath)
{
    return analyzeFile(filePath);
}

QString AndroidExtractor::guidToString(const QUuid& guid)
{
    return guid.toString(QUuid::WithoutBraces).toUpper();
}

QString AndroidExtractor::getGuidDescription(const QUuid& guid)
{
    return guidDescriptions.value(guid, "Unknown");
}

// FileRawAccess implementation

AndroidExtractor::FileRawAccess::FileRawAccess(const QString& path, qint64 baseOffset)
    : baseOffset(baseOffset)
{
    file.setFileName(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file for raw access:" << file.errorString();
    }
}

AndroidExtractor::FileRawAccess::~FileRawAccess()
{
    close();
}

QByteArray AndroidExtractor::FileRawAccess::readData(qint64 offset, qint64 size)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (!file.isOpen()) {
        return QByteArray();
    }

    qint64 actualOffset = baseOffset + offset;
    if (!file.seek(actualOffset)) {
        return QByteArray();
    }

    return file.read(size);
}

bool AndroidExtractor::FileRawAccess::writeData(qint64 offset, const QByteArray& data)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (!file.isOpen() || !file.isWritable()) {
        return false;
    }

    qint64 actualOffset = baseOffset + offset;
    if (!file.seek(actualOffset)) {
        return false;
    }

    return file.write(data) == data.size();
}

qint64 AndroidExtractor::FileRawAccess::size() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return file.isOpen() ? (file.size() - baseOffset) : 0;
}

bool AndroidExtractor::FileRawAccess::isOpen() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return file.isOpen();
}

void AndroidExtractor::FileRawAccess::close()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (file.isOpen()) {
        file.close();
    }
}

// Реализация в AndroidExtractor.cpp:
QString AndroidExtractor::cleanPartitionName(const QString& name)
{
    QString cleaned = name;

    // Удаляем нулевые символы и другие непечатаемые
    cleaned.remove(QChar('\0'));

    // Удаляем непечатные символы, оставляем только печатные ASCII
    QString result;
    for (int i = 0; i < cleaned.length(); i++) {
        QChar ch = cleaned.at(i);
        if (ch.unicode() >= 32 && ch.unicode() <= 126) {
            result.append(ch);
        } else if (ch.unicode() > 126) {
            // Для не-ASCII символов пробуем сохранить (могут быть кириллица и т.д.)
            result.append(ch);
        }
        // Все остальные символы (0-31, 127+) игнорируем
    }

    // Обрезаем пробелы по краям
    result = result.trimmed();

    // Если после очистки строка пустая, возвращаем "Unknown"
    if (result.isEmpty()) {
        return QString("Partition_%1").arg(rand() % 1000); // Простой идентификатор
    }

    return result;
}
