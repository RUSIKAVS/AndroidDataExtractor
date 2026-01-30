#include "PartitionParser.hpp"
#include "KeyParser.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QtEndian>
#include <QByteArray>
#include <QMap>
#include <stdexcept>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>        // для log2
#include <algorithm>    // для std::min, std::max
#include <QtMath>
#include <QProgressDialog>
#include <QApplication>
#include <QProgressBar>


namespace {
QString detectFilesystemType(const QByteArray& header)
{
    if (header.size() < 1024) {
        return "unknown";
    }

    // 1. ext2/3/4 (0xEF53 at offset 0x438)
    if (header.size() >= 0x43A) {
        quint16 magic = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar*>(header.constData() + 0x438));
        if (magic == 0xEF53) {
            // Проверяем версию ext
            if (header.size() >= 0x458) {
                quint32 features = qFromLittleEndian<quint32>(
                    reinterpret_cast<const uchar*>(header.constData() + 0x458));
                if (features & 0x40) return "ext4";
                else if (features & 0x4) return "ext3";
                else return "ext2";
            }
            return "ext4"; // По умолчанию
        }
    }

    // 2. f2fs ("F2FS" at offset 0x400)
    if (header.size() >= 0x404 && header.mid(0x400, 4) == "F2FS") {
        return "f2fs";
    }

    // 3. FAT12/16/32
    if (header.size() >= 0x5A) {
        QString fatStr = QString::fromLatin1(header.mid(0x36, 8).constData()).toUpper();
        if (fatStr.contains("FAT")) {
            // Определяем тип FAT
            quint16 sectorsPerCluster = qFromLittleEndian<quint16>(
                reinterpret_cast<const uchar*>(header.constData() + 0x0D));
            quint16 rootEntries = qFromLittleEndian<quint16>(
                reinterpret_cast<const uchar*>(header.constData() + 0x11));
            quint32 totalSectors = qFromLittleEndian<quint32>(
                reinterpret_cast<const uchar*>(header.constData() + 0x20));

            if (totalSectors == 0) {
                totalSectors = qFromLittleEndian<quint16>(
                    reinterpret_cast<const uchar*>(header.constData() + 0x13));
            }

            quint32 fatSize = totalSectors / sectorsPerCluster;

            if (fatSize < 4085) return "fat12";
            else if (fatSize < 65525) return "fat16";
            else return "fat32";
        }
    }

    // 4. NTFS
    if (header.size() >= 0x0B && header.mid(0x03, 8) == "NTFS    ") {
        return "ntfs";
    }

    // 5. exFAT
    if (header.size() >= 0x40 && header.mid(0x03, 8) == "EXFAT   ") {
        return "exfat";
    }

    // 6. Linux swap
    if (header.size() >= 0x408) {
        QString swapStr = QString::fromLatin1(header.mid(0x400, 10).constData());
        if (swapStr.contains("SWAPSPACE") || swapStr.contains("SWAP-SPACE")) {
            return "swap";
        }
    }

    // 7. Btrfs
    if (header.size() >= 0x10040 && header.mid(0x10040, 8) == "_BHRfS_M") {
        return "btrfs";
    }

    // 8. XFS
    if (header.size() >= 0x414 && header.mid(0x0, 4) == "XFSB") {
        return "xfs";
    }

    // 9. JFFS2
    if (header.size() >= 0x18 && header.mid(0x0, 4) == "\x85\x19\x03\x20") {
        return "jffs2";
    }

    // 10. UBIFS
    if (header.size() >= 0x418 && header.mid(0x0, 4) == "UBI#") {
        return "ubifs";
    }

    // 11. CramFS
    if (header.size() >= 0x20C && header.mid(0x0, 4) == "\x28\xcd\x3d\x45") {
        return "cramfs";
    }

    // 12. SquashFS
    if (header.size() >= 0x20 &&
        (header.mid(0x0, 4) == "sqsh" ||
         header.mid(0x0, 4) == "qshs" ||
         header.mid(0x0, 4) == "hsqs")) {
        return "squashfs";
    }

    // 13. EROFS (Android 13+)
    if (header.size() >= 0x404 && header.mid(0x400, 4) == "\xe2\xe1\xf5\xe0") {
        return "erofs";
    }

    return "unknown";
}

//2


QString decodeGptPartitionName(const QByteArray& nameBytes)
{
    if (nameBytes.isEmpty()) {
        return QString();
    }

    QByteArray cleaned = nameBytes;

    // Удаляем нулевые байты в конце
    while (!cleaned.isEmpty() && cleaned.at(cleaned.size() - 1) == '\0') {
        cleaned.chop(1);
    }

    if (cleaned.isEmpty()) {
        return QString();
    }

    // GPT использует UTF-16LE
    if (cleaned.size() % 2 == 0) {
        bool isLikelyUtf16le = true;
        for (int i = 1; i < cleaned.size(); i += 2) {
            if (cleaned.at(i) != 0) {
                isLikelyUtf16le = false;
                break;
            }
        }

        if (isLikelyUtf16le) {
            // Извлекаем ASCII символы из UTF-16LE
            QByteArray asciiBytes;
            for (int i = 0; i < cleaned.size(); i += 2) {
                asciiBytes.append(cleaned.at(i));
            }
            return QString::fromLatin1(asciiBytes);
        } else {
            // Пробуем как UTF-16LE
            return QString::fromUtf16(
                reinterpret_cast<const char16_t*>(cleaned.constData()),
                cleaned.size() / 2);
        }
    }

    // Пробуем как Latin-1
    return QString::fromLatin1(cleaned);
}

//3

QString cleanPartitionName(const QString& name)
{
    if (name.isEmpty()) {
        return QString("Unknown");
    }

    QString result;

    for (int i = 0; i < name.length(); ++i) {
        QChar ch = name.at(i);

        // Оставляем печатаемые символы
        if (ch.isPrint() || ch == ' ') {
            result.append(ch);
        }
    }

    result = result.trimmed();

    if (result.isEmpty()) {
        return QString("Unnamed");
    }

    return result;
}

//4

bool isEncryptedFilesystem(const QByteArray& header, const QString& partitionName,
                           QString& outEncryptionType)
{
    outEncryptionType.clear();

    if (header.size() < 1024) {
        return false;
    }

    // 1. LUKS/dm-crypt (Full Disk Encryption)
    if (header.startsWith("LUKS\xBA\xBE")) {
        outEncryptionType = "LUKS/dm-crypt (Android FDE)";

        // Парсим детали LUKS
        if (header.size() >= 592) {
            QString version = QString("%1.%2")
            .arg((uchar)header[6])
                .arg((uchar)header[7]);
            QString cipherName = QString::fromLatin1(header.mid(0x60, 32).constData()).trimmed();

            outEncryptionType = QString("LUKS %1 (%2)").arg(version).arg(cipherName);
        }
        return true;
    }

    QString fsType = detectFilesystemType(header);

    // 2. File-Based Encryption (FBE) для известных файловых систем
    if (fsType == "ext4") {
        if (header.size() >= 0x464) {
            quint64 features = qFromLittleEndian<quint64>(
                reinterpret_cast<const uchar*>(header.constData() + 0x460));

            if (features & 0x10000) { // EXT4_FEATURE_INCOMPAT_ENCRYPT
                outEncryptionType = "ext4 File-Based Encryption (FBE)";
                return true;
            }
        }
    }
    else if (fsType == "f2fs") {
        if (header.size() >= 0x406) {
            quint16 feature = qFromLittleEndian<quint16>(
                reinterpret_cast<const uchar*>(header.constData() + 0x404));

            if (feature & 0x0040) { // F2FS_FEATURE_CRYPT
                outEncryptionType = "f2fs File-Based Encryption (FBE)";
                return true;
            }
        }
    }
    else if (fsType == "erofs") {
        // EROFS может поддерживать шифрование
        outEncryptionType = "EROFS with encryption";
        return true;
    }

    // 3. Проверяем эвристически для больших разделов данных
    if (partitionName.toLower().contains("userdata") ||
        partitionName.toLower().contains("data")) {

        // userdata обычно зашифрован в современных Android
        outEncryptionType = "Android Data Encryption (heuristic)";
        return true;
    }

    // 4. Другие системные разделы которые обычно зашифрованы
    static const QStringList encryptedSystemPartitions = {
        "metadata", "persist", "nvdata", "nvcfg", "protect",
        "seccfg", "frp", "tee", "md1img", "spmfw", "scp",
        "sspm", "gz", "lk", "logo"
    };

    QString lowerName = partitionName.toLower();
    for (const QString& part : encryptedSystemPartitions) {
        if (lowerName.contains(part)) {
            outEncryptionType = QString("Android %1 Encryption").arg(part);
            return true;
        }
    }

    return false;
}
}

bool isLikelyEncryptedPartition(const QString& partitionName, qint64 size)
{
    QString lowerName = partitionName.toLower();

    // userdata/data разделы обычно большие и зашифрованы
    if ((lowerName.contains("userdata") ||
         lowerName.contains("data") ||
         lowerName == "userdata") &&
        size > 1LL * 1024 * 1024 * 1024) { // > 1GB
        return true;
    }

    // super раздел (динамический) обычно зашифрован
    if (lowerName == "super" && size > 2LL * 1024 * 1024 * 1024) { // > 2GB
        return true;
    }

    // metadata, persist, nvdata обычно зашифрованы
    static const QStringList encryptedNames = {
        "metadata", "persist", "nvdata", "nvcfg",
        "protect1", "protect2", "seccfg", "frp"
    };

    for (const QString& name : encryptedNames) {
        if (lowerName.contains(name)) {
            return true;
        }
    }

    return false;
}


// Также добавьте функцию cleanPartitionName:

PartitionParser::PartitionParser()
{
}

PartitionParser::ImageType PartitionParser::analyzeImage(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    qint64 fileSize = fileInfo.size();

    qDebug() << "\n=== ANALYZING IMAGE ===";
    qDebug() << "File:" << filePath;
    qDebug() << "Size:" << fileSize << "bytes ("
             << (fileSize / (1024.0 * 1024 * 1024)) << "GB)";

    // ==============================================
    // 0. Проверка на очень большие файлы (raw дампы)
    // ==============================================
    if (fileSize > 5LL * 1024 * 1024 * 1024) { // >5GB
        qDebug() << "Very large file detected, likely raw eMMC dump";
        return RAW_PARTITION;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file for analysis:" << filePath;
        return UNKNOWN;
    }

    // Читаем достаточно данных для анализа различных форматов
    const qint64 READ_SIZE = 65536; // 64KB
    QByteArray header = file.read(READ_SIZE);
    file.close();

    if (header.size() < 512) {
        qDebug() << "File too small for analysis:" << header.size() << "bytes";
        return UNKNOWN;
    }

    // ==============================================
    // 1. Android Sparse Image
    // ==============================================
    if (header.size() >= 28) {
        quint32 magic = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar*>(header.constData()));

        if (magic == 0xED26FF3A) { // Android sparse magic
            qDebug() << "✓ Detected Android sparse image";
            return SPARSE_IMAGE;
        }
    }

    // ==============================================
    // 2. Android Boot/Vendor Boot Images
    // ==============================================
    if (header.startsWith("ANDROID!")) {
        qDebug() << "✓ Detected Android boot image";
        return RAW_PARTITION;
    }

    if (header.startsWith("VNDRBOOT")) {
        qDebug() << "✓ Detected Android vendor boot image";
        return RAW_PARTITION;
    }

    // ==============================================
    // 3. GPT Partition Table
    // ==============================================
    for (int offset = 0; offset <= 16384; offset += 512) {
        if (header.size() >= offset + 512) {
            if (header.mid(offset, 8) == QByteArray("EFI PART")) {
                qDebug() << "✓ Detected GPT at offset 0x" << QString::number(offset, 16);
                return GPT_IMAGE;
            }
        }
    }

    // ==============================================
    // 4. MBR Partition Table
    // ==============================================
    if (header.size() >= 512) {
        if (header.at(510) == char(0x55) && header.at(511) == char(0xAA)) {
            bool isProtectiveMBR = false;
            bool hasValidPartitions = false;

            for (int i = 0; i < 4; i++) {
                int partOffset = 446 + (i * 16);
                if (header.size() >= partOffset + 16) {
                    quint8 partType = static_cast<quint8>(header[partOffset + 4]);

                    if (partType == 0xEE) {
                        isProtectiveMBR = true;
                        break;
                    }

                    if (partType != 0x00) {
                        hasValidPartitions = true;
                    }
                }
            }

            if (!isProtectiveMBR && hasValidPartitions) {
                qDebug() << "✓ Detected MBR with valid partitions";
                return MBR_IMAGE;
            }
        }
    }

    // ==============================================
    // 5. LUKS/dm-crypt Encrypted Volume
    // ==============================================
    if (header.startsWith("LUKS\xBA\xBE")) {
        qDebug() << "✓ Detected LUKS/dm-crypt encrypted volume";
        return RAW_PARTITION;
    }

    // ==============================================
    // 6. File System Detection
    // ==============================================
    QString fsType = detectFilesystemType(header);
    if (fsType != "unknown") {
        qDebug() << "✓ Detected" << fsType << "filesystem";
        return RAW_PARTITION;
    }

    // ==============================================
    // 7. Vendor Specific Formats
    // ==============================================
    if (header.contains("MTK") || header.contains("\x88\x88\x88\x88")) {
        qDebug() << "✓ Detected MediaTek (MTK) specific format";
        return RAW_PARTITION;
    }

    if (header.contains("SAMSUNG") || header.startsWith("SBIN")) {
        qDebug() << "✓ Detected Samsung specific format";
        return RAW_PARTITION;
    }

    if (header.size() >= 0x404 && header.mid(0x400, 4) == QByteArray("\xe2\xe1\xf5\xe0")) {
        qDebug() << "✓ Detected EROFS filesystem (Android 13+)";
        return RAW_PARTITION;
    }

    // ==============================================
    // 8. Эвристический анализ
    // ==============================================

    // Проверяем энтропию данных
    double entropy = calculateEntropy(header.left(4096));
    qDebug() << "  Data entropy:" << entropy;

    // Подсчитываем статистику
    int zeroCount = 0;
    int asciiCount = 0;
    int analyzedBytes = qMin(4096, header.size());

    for (int i = 0; i < analyzedBytes; ++i) {
        uchar c = header[i];
        if (c == 0) zeroCount++;
        if ((c >= 32 && c <= 126) || c == 9 || c == 10 || c == 13) asciiCount++;
    }

    double asciiRatio = (double)asciiCount / analyzedBytes;
    double zeroRatio = (double)zeroCount / analyzedBytes;

    qDebug() << "  Zero bytes:" << zeroCount << "/" << analyzedBytes
             << "(" << (zeroRatio * 100) << "%)";
    qDebug() << "  ASCII bytes:" << asciiCount << "/" << analyzedBytes
             << "(" << (asciiRatio * 100) << "%)";

    // Эвристики для определения типа
    if (entropy > 7.5) {
        qDebug() << "  ! High entropy - likely encrypted";
        return RAW_PARTITION;
    }

    if (asciiRatio < 0.05 && zeroRatio < 0.8 && fileSize > 10 * 1024 * 1024) {
        qDebug() << "✓ Heuristic: likely raw partition";
        return RAW_PARTITION;
    }

    // ==============================================
    // 9. Проверка по размеру
    // ==============================================
    if (fileSize > 50 * 1024 * 1024) {
        qDebug() << "✓ Large file, treating as raw partition";
        return RAW_PARTITION;
    }

    // ==============================================
    // 10. Неизвестный формат
    // ==============================================
    qDebug() << "✗ Unknown image type";

    // Дополнительная отладочная информация
    qDebug() << "\n=== DEBUG INFO ===";
    qDebug() << "Entropy:" << entropy << "/ 8.0";

    if (isLikelyEncrypted(header)) {
        qDebug() << "⚠ Data appears to be encrypted";
    }

    return UNKNOWN;
}

QList<AndroidPartition> PartitionParser::parseImage(const QString& filePath)
{
    ImageType type = analyzeImage(filePath);
    qDebug() << "Image type:" << imageTypeToString(type) << "file:" << filePath;

    switch (type) {
    case GPT_IMAGE:
        return parseGPT(filePath);
    case MBR_IMAGE:
        return parseMBR(filePath);
    case SPARSE_IMAGE:
        return parseSparseImage(filePath);
    case RAW_PARTITION: {
        QFileInfo fi(filePath);
        qint64 fileSize = fi.size();

        if (fileSize > 1LL * 1024 * 1024 * 1024) { // >1GB
            qDebug() << "Large raw file (" << (fileSize / (1024.0*1024*1024))
                     << "GB), using raw dump parser";
            return parseRawDump(filePath);
        } else {
            return { analyzeRawFile(filePath) };
        }
    }
    default:
        qWarning() << "Unknown image type for file:" << filePath;
        return {};
    }
}

quint64 PartitionParser::readUInt64(QFile& file, qint64 offset)
{
    if (!file.seek(offset)) {
        return 0;
    }

    QByteArray data = file.read(8);
    if (data.size() != 8) {
        return 0;
    }

    return qFromLittleEndian<quint64>(
        reinterpret_cast<const uchar*>(data.constData()));
}

quint32 PartitionParser::readUInt32(QFile& file, qint64 offset)
{
    if (!file.seek(offset)) {
        return 0;
    }

    QByteArray data = file.read(4);
    if (data.size() != 4) {
        return 0;
    }

    return qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar*>(data.constData()));
}

quint16 PartitionParser::readUInt16(QFile& file, qint64 offset)
{
    if (!file.seek(offset)) {
        return 0;
    }

    QByteArray data = file.read(2);
    if (data.size() != 2) {
        return 0;
    }

    return qFromLittleEndian<quint16>(
        reinterpret_cast<const uchar*>(data.constData()));
}

QList<AndroidPartition> PartitionParser::parseGPT(const QString& filePath)
{
    QList<AndroidPartition> partitions;
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file for GPT parsing:" << filePath;
        return partitions;
    }

    // Читаем GPT header
    file.seek(512);
    QByteArray header = file.read(512);

    if (header.size() < 512) {
        qWarning() << "Cannot read GPT header";
        file.close();
        return partitions;
    }

    // Проверяем сигнатуру
    if (header.mid(0, 8) != QByteArray("EFI PART")) {
        qWarning() << "Invalid GPT signature";
        file.close();
        return partitions;
    }

    // Получаем информацию из заголовка
    quint32 numParts = qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar*>(header.constData() + 80));

    quint32 partEntrySize = qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar*>(header.constData() + 84));

    quint64 partTableLBA = qFromLittleEndian<quint64>(
        reinterpret_cast<const uchar*>(header.constData() + 72));

    qint64 partTableOffset = partTableLBA * 512;

    qDebug() << "GPT info: partitions =" << numParts
             << ", entry size =" << partEntrySize
             << ", table offset = 0x" << QString::number(partTableOffset, 16);

    // Читаем таблицу разделов
    file.seek(partTableOffset);
    QByteArray partitionTable = file.read(numParts * partEntrySize);

    if (partitionTable.size() < static_cast<int>(numParts * partEntrySize)) {
        qWarning() << "Cannot read partition table";
        file.close();
        return partitions;
    }

    // Парсим разделы
    for (quint32 i = 0; i < numParts; ++i) {
        qint64 entryOffset = i * partEntrySize;
        QByteArray entry = partitionTable.mid(entryOffset, partEntrySize);

        if (entry.size() < static_cast<int>(partEntrySize)) {
            continue;
        }

        // Проверяем GUID типа раздела
        QByteArray typeGuid = entry.left(16);
        if (typeGuid.count('\0') == 16) {
            continue; // Пустая запись
        }

        // Читаем LBA
        quint64 startLBA = qFromLittleEndian<quint64>(
            reinterpret_cast<const uchar*>(entry.constData() + 32));
        quint64 endLBA = qFromLittleEndian<quint64>(
            reinterpret_cast<const uchar*>(entry.constData() + 40));

        if (startLBA == 0 || endLBA < startLBA) {
            continue;
        }

        AndroidPartition part;
        part.offset = startLBA * 512;
        part.size = (endLBA - startLBA + 1) * 512;
        part.filePath = filePath;
        part.isEncrypted = false;

        // Получаем и декодируем имя
        QByteArray nameBytes = entry.mid(56, 72);
        part.name = decodeGptPartitionName(nameBytes);
        part.name = cleanPartitionName(part.name);

        // Если имя пустое, создаем стандартное
        if (part.name.isEmpty() || part.name == "Unnamed" || part.name == "Unknown") {
            part.name = QString("Partition_%1").arg(i + 1);
        }

        // Определяем тип раздела
        QString typeGuidHex = typeGuid.toHex().toUpper();
        if (typeGuidHex.startsWith("EBD0A0A2")) {
            part.type = "Basic Data";
        } else if (typeGuidHex.startsWith("C12A7328")) {
            part.type = "EFI System";
        } else if (typeGuidHex.startsWith("21686148")) {
            part.type = "BIOS Boot";
        } else if (typeGuidHex.startsWith("0FC63DAF")) {
            part.type = "Linux";
        } else {
            part.type = "GPT";
        }

        if (part.isValid()) {
            partitions.append(part);
            qDebug() << "Found GPT partition:" << part.name
                     << "offset: 0x" << QString::number(part.offset, 16)
                     << "size:" << part.size << "bytes";
        }
    }

    file.close();
    return partitions;
}

QList<AndroidPartition> PartitionParser::parseMBR(const QString& filePath)
{
    QList<AndroidPartition> partitions;
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        return partitions;
    }

    // Читаем MBR
    file.seek(0);
    QByteArray mbr = file.read(512);

    if (mbr.size() < 512) {
        file.close();
        return partitions;
    }

    // Проверяем сигнатуру MBR
    if (mbr.at(510) != char(0x55) || mbr.at(511) != char(0xAA)) {
        file.close();
        return partitions;
    }

    // Парсим 4 primary раздела
    for (int i = 0; i < 4; ++i) {
        int offset = 446 + (i * 16);

        // Статус раздела
        quint8 status = static_cast<quint8>(mbr[offset]);
        if (status == 0x00) {
            continue; // Неактивный раздел
        }

        // Тип раздела
        quint8 partType = static_cast<quint8>(mbr[offset + 4]);
        if (partType == 0x00) {
            continue; // Пустой раздел
        }

        // Смещение в секторах
        quint32 startSector = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar*>(mbr.constData() + offset + 8));

        // Количество секторов
        quint32 numSectors = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar*>(mbr.constData() + offset + 12));

        if (startSector == 0 || numSectors == 0) {
            continue;
        }

        AndroidPartition part;
        part.offset = startSector * 512;
        part.size = numSectors * 512;
        part.filePath = filePath;
        part.isEncrypted = false;

        // Определяем имя по типу раздела
        switch (partType) {
        case 0x83: part.name = "Linux"; break;
        case 0x82: part.name = "Linux Swap"; break;
        case 0x07: part.name = "NTFS/HPFS"; break;
        case 0x0B: case 0x0C: part.name = "FAT32"; break;
        case 0x05: case 0x0F: part.name = "Extended"; break;
        case 0xEE: part.name = "GPT Protective"; break;
        default:
            part.name = QString("Type_0x%1").arg(partType, 2, 16, QChar('0'));
            break;
        }

        part.name = QString("%1_P%2").arg(part.name).arg(i + 1);
        part.type = "MBR";

        if (part.isValid()) {
            partitions.append(part);
            qDebug() << "Found MBR partition:" << part.name
                     << "offset: 0x" << QString::number(part.offset, 16)
                     << "size:" << part.size << "bytes";
        }
    }

    file.close();
    return partitions;
}

QList<AndroidPartition> PartitionParser::parseSparseImage(const QString& filePath)
{
    QList<AndroidPartition> partitions;
    qWarning() << "Sparse image parsing not implemented yet for:" << filePath;
    return partitions;
}

AndroidPartition PartitionParser::analyzeRawFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    AndroidPartition part;

    part.name = fileInfo.fileName();
    part.offset = 0;
    part.size = fileInfo.size();
    part.filePath = filePath;
    part.type = "Raw";
    part.isEncrypted = false;

    // Проверяем известные форматы Android
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray header = file.read(4096);
        file.close();

        if (header.size() >= 8) {
            // boot.img
            if (header.startsWith("ANDROID!")) {
                part.type = "Android Boot Image";
            }
            // vendor_boot.img
            else if (header.startsWith("VNDRBOOT")) {
                part.type = "Android Vendor Boot";
            }
            // sparse image
            else if (header.size() >= 28) {
                quint32 magic = qFromLittleEndian<quint32>(
                    reinterpret_cast<const uchar*>(header.constData()));
                if (magic == 0xED26FF3A) {
                    part.type = "Android Sparse";
                }
            }
        }
    }

    return part;
}

QList<AndroidPartition> PartitionParser::scanDirectory(const QString& dirPath,
                                                       KeyParser* keyParser)
{
    QList<AndroidPartition> partitions;
    QDir dir(dirPath);

    if (!dir.exists()) {
        qWarning() << "Directory does not exist:" << dirPath;
        return partitions;
    }

    qDebug() << "Scanning directory:" << dirPath;

    QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);

    // Сначала ищем большие файлы (образы) - сортируем по размеру
    QList<QPair<qint64, QString>> sizedFiles;
    for (const QString& file : files) {
        QString fullPath = dir.absoluteFilePath(file);
        qint64 fileSize = QFileInfo(fullPath).size();
        sizedFiles.append(qMakePair(fileSize, file));
    }

    // Сортируем по размеру (от большего к меньшему)
    std::sort(sizedFiles.begin(), sizedFiles.end(),
              [](const QPair<qint64, QString>& a, const QPair<qint64, QString>& b) {
                  return a.first > b.first; // Сортировка по убыванию
              });

    // Обновляем files в отсортированном порядке
    files.clear();
    for (const auto& pair : sizedFiles) {
        files.append(pair.second);
        qDebug() << "File:" << pair.second << "size:" << pair.first << "bytes";
    }

    for (const QString& file : files) {
        QString fullPath = dir.absoluteFilePath(file);
        qint64 fileSize = QFileInfo(fullPath).size();

        // Пропускаем keys.json
        if (file.endsWith("keys.json", Qt::CaseInsensitive)) {
            continue;
        }

        qDebug() << "\n========================================";
        qDebug() << "Processing file:" << file << "size:" << fileSize << "bytes";

        ImageType type = analyzeImage(fullPath);
        qDebug() << "Detected type:" << imageTypeToString(type);

        QList<AndroidPartition> imageParts;

        if (type == UNKNOWN) {
            // Для больших файлов пробуем эвристический поиск
            if (fileSize > 50 * 1024 * 1024) { // >50MB
                qDebug() << "Trying heuristic scan for large file";
                imageParts = scanForPartitionsHeuristic(fullPath);

                if (imageParts.isEmpty()) {
                    AndroidPartition part = analyzeRawFile(fullPath);
                    if (part.isValid()) {
                        imageParts.append(part);
                    }
                }
            } else {
                AndroidPartition part = analyzeRawFile(fullPath);
                if (part.isValid()) {
                    imageParts.append(part);
                }
            }
        } else {
            imageParts = parseImage(fullPath);

            // Если стандартный парсинг не нашел разделов, пробуем эвристический
            if (imageParts.isEmpty() && fileSize > 100 * 1024 * 1024) {
                qDebug() << "Standard parse failed, trying heuristic";
                imageParts = scanForPartitionsHeuristic(fullPath);
            }
        }

        // Анализируем каждый раздел на шифрование
        if (keyParser && !imageParts.isEmpty()) {
            qDebug() << "Analyzing" << imageParts.size() << "partitions for encryption";
            for (auto& part : imageParts) {
                part = analyzePartitionWithKeys(part, keyParser);
            }
        }

        partitions.append(imageParts);

        if (!imageParts.isEmpty()) {
            qDebug() << "Found" << imageParts.size() << "partitions in" << file;
        }
    }

    qDebug() << "\n========================================";
    qDebug() << "Total partitions found:" << partitions.size();

    // Сортируем по offset
    std::sort(partitions.begin(), partitions.end(),
              [](const AndroidPartition& a, const AndroidPartition& b) {
                  return a.offset < b.offset;
              });

    return partitions;
}

bool PartitionParser::extractAndDecryptPartition(const AndroidPartition& partition,
                                                 const QString& outputPath,
                                                 const QByteArray& decryptionKey)
{
    QFile sourceFile(partition.filePath);
    QFile destFile(outputPath);

    if (!sourceFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open source file:" << partition.filePath;
        return false;
    }

    if (!destFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Cannot open destination file:" << outputPath;
        sourceFile.close();
        return false;
    }

    bool success = false;

    // Читаем заголовок для определения типа шифрования
    sourceFile.seek(partition.offset);
    QByteArray header = sourceFile.read(4096);
    sourceFile.seek(partition.offset); // Возвращаемся к началу

    QString encryptionType = detectEncryptionType(header);
    qDebug() << "Detected encryption type:" << encryptionType;

    if (encryptionType.contains("LUKS")) {
        success = decryptLUKSPartition(sourceFile, destFile, partition, decryptionKey);
    } else if (encryptionType.contains("MTK")) {
        success = decryptMTKPartition(sourceFile, destFile, partition, decryptionKey);
    } else if (encryptionType.contains("FBE")) {
        success = decryptFBEPartition(sourceFile, destFile, partition, decryptionKey);
    } else {
        // Пробуем общее дешифрование
        success = decryptGeneric(sourceFile, destFile, partition, decryptionKey);
    }

    sourceFile.close();
    destFile.close();

    if (!success) {
        destFile.remove();
        qWarning() << "Failed to decrypt partition";
    }

    return success;
}

QString PartitionParser::imageTypeToString(ImageType type)
{
    switch (type) {
    case UNKNOWN: return "Unknown";
    case GPT_IMAGE: return "GPT Image";
    case MBR_IMAGE: return "MBR Image";
    case RAW_PARTITION: return "Raw Partition";
    case SPARSE_IMAGE: return "Sparse Image";
    default: return "Invalid";
    }
}

AndroidPartition PartitionParser::analyzePartitionWithKeys(const AndroidPartition& partition,
                                                           KeyParser* keyParser)
{
    AndroidPartition result = partition;

    if (!keyParser) {
        return result;
    }

    // Проверяем наличие ключей
    result.hasRpmbKey = !keyParser->getRpmbKey().isEmpty();
    result.hasFdeKey = !keyParser->getFdeKey().isEmpty();
    result.hasMeId = !keyParser->getMeId().isEmpty();

    // Читаем заголовок раздела
    QFile file(partition.filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return result;
    }

    if (!file.seek(partition.offset)) {
        file.close();
        return result;
    }

    // Читаем больше данных для анализа (первые 128KB)
    QByteArray header = file.read(131072);
    file.close();

    // Определяем тип файловой системы
    QString fsType = detectFilesystemType(header);
    if (fsType != "unknown") {
        result.type = fsType.toUpper();
    }

    // Анализируем шифрование
    QString encryptionType;
    result.isEncrypted = isEncryptedFilesystem(header, partition.name, encryptionType);

    if (result.isEncrypted) {
        result.encryptionType = encryptionType;

        // Автоматически назначаем ключи в зависимости от типа раздела
        QString lowerName = partition.name.toLower();

        // Для data разделов
        if (lowerName.contains("userdata") ||
            lowerName.contains("data") ||
            lowerName == "super") {

            if (result.hasFdeKey) {
                result.keyId = "MTK_FDEKEY";
                result.encryptionType += " (FDE)";
            } else if (result.hasRpmbKey) {
                result.keyId = "MTK_RPMBKEY";
                result.encryptionType += " (RPMB-based)";
                qDebug() << "Info: Using RPMB key for" << partition.name
                         << "(FDE key not available)";
            }
        }
        // Для security разделов
        else if (lowerName.contains("sec") ||
                 lowerName.contains("rpmb") ||
                 lowerName.contains("protect") ||
                 lowerName.contains("metadata") ||
                 lowerName.contains("persist") ||
                 lowerName.contains("nvdata")) {

            if (result.hasRpmbKey) {
                result.keyId = "MTK_RPMBKEY";
                result.encryptionType += " (RPMB)";
            }
        }
        // Для firmware разделов
        else if (lowerName.contains("md1img") ||
                 lowerName.contains("spmfw") ||
                 lowerName.contains("scp") ||
                 lowerName.contains("sspm") ||
                 lowerName.contains("gz") ||
                 lowerName.contains("lk") ||
                 lowerName.contains("tee") ||
                 lowerName.contains("logo")) {

            if (result.hasRpmbKey) {
                result.keyId = "MTK_RPMBKEY";
                result.encryptionType += " (Firmware)";
            }
        }

        // Если ключ не назначен, но есть RPMB - используем как запасной
        if (result.keyId.isEmpty() && result.hasRpmbKey) {
            result.keyId = "MTK_RPMBKEY";
            result.encryptionType += " (Generic)";
        }

        if (result.canBeDecrypted()) {
            qDebug() << "✓" << partition.name << "->" << result.encryptionType
                     << "using" << result.keyId;
        }
    } else {
        qDebug() << "✗" << partition.name << "-> Not encrypted";
    }

    return result;
}


bool PartitionParser::decryptLUKSPartition(QFile& source, QFile& dest,
                                           const AndroidPartition& partition,
                                           const QByteArray& key)
{
    qDebug() << "Decrypting LUKS partition:" << partition.name;

    // Пока реализуем простое копирование
    // В реальности нужно парсить LUKS заголовок и дешифровать
    return extractPartitionSimple(partition, dest.fileName());
}

bool PartitionParser::decryptMTKPartition(QFile& source, QFile& dest,
                                          const AndroidPartition& partition,
                                          const QByteArray& key)
{
    qDebug() << "Decrypting MTK partition:" << partition.name
             << "with key:" << key.toHex().left(16) << "...";

    // Пока реализуем простое копирование
    // В реальности нужно использовать MTK специфичные алгоритмы
    return extractPartitionSimple(partition, dest.fileName());
}


// ============================================================================
// Функции для определения типа шифрования
// ============================================================================

QString PartitionParser::detectEncryptionType(const QByteArray& header)
{
    if (header.size() < 1024) {
        return "Unknown (header too small)";
    }

    // 1. Проверяем LUKS (Android FDE)
    if (header.startsWith("LUKS\xBA\xBE")) {
        return "LUKS/dm-crypt (Android FDE)";
    }

    // 2. Проверяем MTK шифрование
    if (isMTKEncrypted(header)) {
        return "MTK Crypto";
    }

    // 3. Проверяем File-Based Encryption
    QString fsType = detectFilesystemType(header);
    if ((fsType == "ext4" || fsType == "f2fs") && isFBEEncrypted(header)) {
        return "FBE (ext4/f2fs)";
    }

    // 4. Проверяем Android FDE (старая версия)
    if (isAndroidFDEEncrypted(header)) {
        return "Android FDE (legacy)";
    }

    // 5. Эвристическая проверка
    if (header.mid(0, 16).toHex().count("0") < 10) {
        // Если первые 16 байт не пустые, возможно зашифровано
        return "Possibly encrypted (heuristic)";
    }

    return "Not encrypted";
}

bool PartitionParser::isLUKSEncrypted(const QByteArray& header)
{
    return header.size() >= 8 && header.startsWith("LUKS\xBA\xBE");
}

bool PartitionParser::isMTKEncrypted(const QByteArray& header)
{
    if (header.size() < 512) return false;

    // MTK часто добавляет свои сигнатуры
    static const QByteArray mtkSignatures[] = {
        QByteArray("MMM"),  // MediaTek Modem
        QByteArray("MTK"),
        QByteArray("\x88\x88\x88\x88", 4),
        QByteArray("\xCE\xCE\xCE\xCE", 4)
    };


    for (const auto& sig : mtkSignatures) {
        if (header.contains(sig)) {
            return true;
        }
    }

    // Проверяем на специфичные MTK структуры
    if (header.size() >= 256) {
        // MTK часто использует определенные паттерны
        quint32 magic1 = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar*>(header.constData()));
        quint32 magic2 = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar*>(header.constData() + 4));

        if (magic1 == 0x4D4D4D4D || magic2 == 0x4B544D) { // "MMM" или "MTK"
            return true;
        }
    }

    return false;
}

bool PartitionParser::isFBEEncrypted(const QByteArray& header)
{
    if (header.size() < 0x464) return false;

    QString fsType = detectFilesystemType(header);

    if (fsType == "ext4") {
        // Проверяем флаг шифрования в superblock
        quint64 features = qFromLittleEndian<quint64>(
            reinterpret_cast<const uchar*>(header.constData() + 0x460));
        return (features & 0x10000) != 0; // EXT4_FEATURE_INCOMPAT_ENCRYPT
    }
    else if (fsType == "f2fs") {
        if (header.size() >= 0x406) {
            quint16 feature = qFromLittleEndian<quint16>(
                reinterpret_cast<const uchar*>(header.constData() + 0x404));
            return (feature & 0x0040) != 0; // F2FS_FEATURE_CRYPT
        }
    }

    return false;
}

bool PartitionParser::isAndroidFDEEncrypted(const QByteArray& header)
{
    if (header.size() < 16) return false;

    // Старое Android FDE может иметь различные сигнатуры
    // Проверяем на наличие известных Android crypto структур

    // Эвристика: если данные выглядят случайными
    int asciiCount = 0;
    for (int i = 0; i < qMin(256, header.size()); ++i) {
        uchar c = header[i];
        if ((c >= 32 && c <= 126) || c == 9 || c == 10 || c == 13) {
            asciiCount++;
        }
    }

    // Если меньше 10% ASCII символов, возможно зашифровано
    return (asciiCount * 100 / qMin(256, header.size())) < 10;
}

// ============================================================================
// Функции дешифрования
// ============================================================================





bool PartitionParser::decryptFBEPartition(QFile& source, QFile& dest,
                                          const AndroidPartition& partition,
                                          const QByteArray& key)
{
    qDebug() << "Decrypting FBE partition:" << partition.name;

    // File-Based Encryption требует сложной обработки
    // Каждый файл зашифрован отдельно
    return extractPartitionSimple(partition, dest.fileName());
}

bool PartitionParser::decryptGeneric(QFile& source, QFile& dest,
                                     const AndroidPartition& partition,
                                     const QByteArray& key)
{
    qDebug() << "Attempting generic decryption for:" << partition.name;

    if (key.isEmpty()) {
        qWarning() << "No decryption key provided";
        return extractPartitionSimple(partition, dest.fileName());
    }

    // Простая XOR дешифровка для демонстрации
    // В реальности нужно определять алгоритм и режим

    source.seek(partition.offset);
    qint64 bytesRemaining = partition.size;
    const qint64 bufferSize = 1024 * 1024;

    while (bytesRemaining > 0) {
        qint64 bytesToRead = qMin(bytesRemaining, bufferSize);
        QByteArray buffer = source.read(bytesToRead);

        if (buffer.size() != bytesToRead) {
            qWarning() << "Read error at position" << source.pos();
            return false;
        }

        // Простой XOR для демонстрации
        QByteArray decrypted = buffer;
        for (int i = 0; i < decrypted.size(); ++i) {
            decrypted[i] = decrypted[i] ^ key[i % key.size()];
        }

        if (dest.write(decrypted) != decrypted.size()) {
            qWarning() << "Write error";
            return false;
        }

        bytesRemaining -= buffer.size();
    }

    return true;
}

bool PartitionParser::extractPartitionSimple(const AndroidPartition& partition,
                                             const QString& outputPath)
{
    QFile sourceFile(partition.filePath);
    QFile destFile(outputPath);

    if (!sourceFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open source file:" << partition.filePath;
        return false;
    }

    if (!destFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Cannot open destination file:" << outputPath;
        sourceFile.close();
        return false;
    }

    bool success = false;

    try {
        if (!sourceFile.seek(partition.offset)) {
            throw std::runtime_error("Cannot seek to partition offset");
        }

        qint64 bytesRemaining = partition.size;
        const qint64 bufferSize = 1024 * 1024;

        while (bytesRemaining > 0) {
            qint64 bytesToRead = qMin(bytesRemaining, bufferSize);
            QByteArray buffer = sourceFile.read(bytesToRead);

            if (buffer.size() != bytesToRead) {
                throw std::runtime_error("Read error");
            }

            if (destFile.write(buffer) != buffer.size()) {
                throw std::runtime_error("Write error");
            }

            bytesRemaining -= buffer.size();
        }

        destFile.flush();
        success = true;
        qDebug() << "Successfully extracted partition to:" << outputPath;

    } catch (const std::exception& e) {
        qWarning() << "Error extracting partition:" << e.what();
        success = false;
    }

    sourceFile.close();
    destFile.close();

    if (!success) {
        destFile.remove();
    }

    return success;
}

// ============================================================================
// Обновляем существующий метод extractPartition
// ============================================================================

bool PartitionParser::extractPartition(const AndroidPartition& partition,
                                       const QString& outputPath,
                                       const QByteArray& decryptionKey)
{
    QFile sourceFile(partition.filePath);
    QFile destFile(outputPath);

    if (!sourceFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open source file:" << partition.filePath;
        return false;
    }

    if (!destFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Cannot open destination file:" << outputPath;
        sourceFile.close();
        return false;
    }

    bool success = false;

    // Новый код для дешифрования
    if (!decryptionKey.isEmpty() && partition.isEncrypted) {
        sourceFile.close();
        destFile.close();
        return extractAndDecryptPartition(partition, outputPath, decryptionKey);
    }

    // Старый код для недешифрованного извлечения
    try {
        if (!sourceFile.seek(partition.offset)) {
            throw std::runtime_error("Cannot seek to partition offset");
        }

        qint64 bytesRemaining = partition.size;
        const qint64 bufferSize = 1024 * 1024;
        QByteArray buffer;

        bool useDecryption = !decryptionKey.isEmpty() && partition.isEncrypted;

        if (useDecryption) {
            qDebug() << "Decrypting partition with key:"
                     << decryptionKey.toHex().left(16) << "...";
        }

        while (bytesRemaining > 0) {
            qint64 bytesToRead = qMin(bytesRemaining, bufferSize);
            buffer = sourceFile.read(bytesToRead);

            if (buffer.size() != bytesToRead) {
                throw std::runtime_error("Read error");
            }

            // TODO: Реализовать фактическую дешифровку
            // Для примера оставляем как есть

            if (destFile.write(buffer) != buffer.size()) {
                throw std::runtime_error("Write error");
            }

            bytesRemaining -= buffer.size();
        }

        destFile.flush();
        success = true;
        qDebug() << "Successfully extracted" << (useDecryption ? "and decrypted " : "")
                 << "partition to:" << outputPath;

    } catch (const std::exception& e) {
        qWarning() << "Error extracting partition:" << e.what();
        success = false;
    }

    sourceFile.close();
    destFile.close();

    if (!success) {
        destFile.remove();
    }

    return success;
}

QList<AndroidPartition> PartitionParser::scanForPartitionsHeuristic(const QString& filePath)
{
    QList<AndroidPartition> partitions;
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        return partitions;
    }

    qint64 fileSize = file.size();
    qDebug() << "Heuristic scan of file:" << filePath << "size:" << fileSize;

    // Типичные смещения для Android разделов
    QVector<qint64> commonOffsets = {
        0x0,           // MBR/GPT
        0x8000,        // 32KB
        0x40000,       // 256KB
        0x80000,       // 512KB
        0xC0000,       // 768KB
        0x100000,      // 1MB
        0x200000,      // 2MB
        0x400000,      // 4MB
        0x800000,      // 8MB
        0x1000000,     // 16MB
        0x2000000,     // 32MB
        0x4000000,     // 64MB
        0x8000000,     // 128MB
        0x10000000,    // 256MB
        0x20000000,    // 512MB
        0x40000000,    // 1GB
        0x80000000     // 2GB
    };

    const qint64 READ_SIZE = 4096;
    QByteArray buffer;

    for (qint64 offset : commonOffsets) {
        if (offset >= fileSize) continue;

        if (!file.seek(offset)) continue;

        buffer = file.read(READ_SIZE);
        if (buffer.size() < READ_SIZE) continue;

        QString fsType = detectFilesystemType(buffer);

        if (fsType != "unknown") {
            AndroidPartition part;
            part.name = QString("Heuristic_%1").arg(partitions.size() + 1);
            part.offset = offset;
            part.size = determinePartitionSize(file, offset, fileSize);
            part.filePath = filePath;
            part.type = fsType.toUpper();
            part.isEncrypted = false;

            QString guessedName = guessPartitionName(buffer, offset);
            if (!guessedName.isEmpty()) {
                part.name = guessedName;
            }

            if (part.isValid()) {
                partitions.append(part);
                qDebug() << "Found partition heuristically:"
                         << part.name << "at offset" << offset
                         << "size:" << part.size << "type:" << part.type;
            }
        }
    }

    file.close();
    return partitions;
}

qint64 PartitionParser::determinePartitionSize(QFile& file, qint64 offset, qint64 maxSize)
{
    if (!file.seek(offset)) return 0;

    QByteArray header = file.read(8192);
    if (header.size() < 4096) return 0;

    QString fsType = detectFilesystemType(header);

    // Для известных файловых систем пытаемся определить точный размер

    // 1. EXT2/3/4
    if (fsType.startsWith("ext")) {
        if (header.size() >= 0x458) {
            quint32 blockCount = qFromLittleEndian<quint32>(
                reinterpret_cast<const uchar*>(header.constData() + 0x454));
            quint32 blockSize = 1024 << (qFromLittleEndian<quint32>(
                                             reinterpret_cast<const uchar*>(header.constData() + 0x418)) & 0xF);

            if (blockCount > 0 && blockSize > 0) {
                qint64 calculatedSize = qint64(blockCount) * blockSize;
                qDebug() << "EXT filesystem: blocks =" << blockCount
                         << "block size =" << blockSize
                         << "total =" << calculatedSize;
                return calculatedSize;
            }
        }
    }

    // 2. F2FS
    else if (fsType == "f2fs") {
        if (header.size() >= 0x458) {
            quint64 segmentCount = qFromLittleEndian<quint64>(
                reinterpret_cast<const uchar*>(header.constData() + 0x450));
            quint32 blockSize = 4096; // F2FS обычно использует 4KB блоки

            if (segmentCount > 0) {
                // В F2FS segment обычно 2MB
                qint64 calculatedSize = segmentCount * 2 * 1024 * 1024;
                qDebug() << "F2FS: segments =" << segmentCount
                         << "calculated size =" << calculatedSize;
                return calculatedSize;
            }
        }
    }

    // 3. LUKS - определяем по заголовку
    else if (header.startsWith("LUKS\xBA\xBE")) {
        if (header.size() >= 592) {
            // Пытаемся определить размер из LUKS заголовка
            // payload offset обычно 0x4000 (16384) или 0x10000 (65536)
            qint64 dataOffset = 0x4000;

            // Ищем следующую сигнатуру после LUKS
            return findNextSignature(file, offset + dataOffset, maxSize) - offset;
        }
    }

    // 4. Эвристика: ищем следующую сигнатуру файловой системы
    return findNextSignature(file, offset, maxSize) - offset;
}

QString PartitionParser::guessPartitionName(const QByteArray& header, qint64 offset)
{
    Q_UNUSED(offset); // Пока не используем offset

    QString data = QString::fromLatin1(header.constData(), qMin(1024, header.size()));

    // Проверяем наличие ключевых строк
    if (data.contains("userdata", Qt::CaseInsensitive)) {
        return "userdata";
    }

    if (data.contains("system", Qt::CaseInsensitive)) {
        return "system";
    }

    if (data.contains("vendor", Qt::CaseInsensitive)) {
        return "vendor";
    }

    if (data.contains("boot", Qt::CaseInsensitive) ||
        header.startsWith("ANDROID!")) {
        return "boot";
    }

    if (data.contains("recovery", Qt::CaseInsensitive)) {
        return "recovery";
    }

    if (data.contains("cache", Qt::CaseInsensitive)) {
        return "cache";
    }

    if (data.contains("metadata", Qt::CaseInsensitive)) {
        return "metadata";
    }

    if (data.contains("persist", Qt::CaseInsensitive)) {
        return "persist";
    }

    return QString();
}

QList<AndroidPartition> PartitionParser::parseRawDump(const QString& filePath)
{
    QList<AndroidPartition> partitions;
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open raw dump:" << filePath;
        return partitions;
    }

    qint64 fileSize = file.size();
    qDebug() << "\n=== PARSING RAW DUMP ===";
    qDebug() << "File:" << filePath;
    qDebug() << "Size:" << fileSize << "bytes ("
             << (fileSize / (1024.0 * 1024 * 1024)) << "GB)";

    // 1. Сначала пробуем стандартный GPT парсинг
    qDebug() << "Trying GPT parsing...";
    partitions = parseGPT(filePath);

    if (!partitions.isEmpty()) {
        qDebug() << "Found" << partitions.size() << "partitions via GPT";
        file.close();
        return partitions;
    }

    // 2. Пробуем MBR парсинг
    qDebug() << "Trying MBR parsing...";
    partitions = parseMBR(filePath);

    if (!partitions.isEmpty()) {
        qDebug() << "Found" << partitions.size() << "partitions via MBR";
        file.close();
        return partitions;
    }

    // 3. Для MTK устройств пробуем специфичный парсер
    qDebug() << "Trying MTK-specific parsing...";
    partitions = parseMtkRawDump(filePath);

    if (!partitions.isEmpty()) {
        qDebug() << "Found" << partitions.size() << "MTK partitions";
        file.close();
        return partitions;
    }

    // 4. Интенсивный поиск разделов по всему файлу
    qDebug() << "Starting intensive partition search...";

    const qint64 SCAN_STEP = 512 * 1024; // 512KB шаг для ускорения
    const qint64 READ_SIZE = 8192; // 8KB для анализа
    QByteArray buffer;

    qint64 lastProgressUpdate = 0;

    for (qint64 offset = 0; offset < fileSize - READ_SIZE; offset += SCAN_STEP) {
        // Выводим прогресс в консоль каждые 5%
        int currentPercent = static_cast<int>((offset * 100) / fileSize);
        if (currentPercent >= lastProgressUpdate + 5) {
            lastProgressUpdate = currentPercent;
            qDebug() << "Scan progress:" << currentPercent << "%";
        }

        if (!file.seek(offset)) continue;

        buffer = file.read(READ_SIZE);
        if (buffer.size() < 1024) continue;

        // Проверяем различные сигнатуры
        bool foundPartition = false;
        AndroidPartition part;

        // a) Проверяем файловую систему
        QString fsType = detectFilesystemType(buffer);
        if (fsType != "unknown") {
            foundPartition = true;
            part.type = fsType.toUpper();
        }

        // b) Проверяем LUKS
        else if (buffer.startsWith("LUKS\xBA\xBE")) {
            foundPartition = true;
            part.type = "LUKS";
            part.isEncrypted = true;
        }

        // c) Проверяем Android boot
        else if (buffer.startsWith("ANDROID!")) {
            foundPartition = true;
            part.type = "Android Boot";
        }

        // d) Проверяем vendor boot
        else if (buffer.startsWith("VNDRBOOT")) {
            foundPartition = true;
            part.type = "Android Vendor Boot";
        }

        // e) Проверяем на высокую энтропию (возможно зашифровано)
        else if (calculateEntropy(buffer.left(4096)) > 7.5) {
            foundPartition = true;
            part.type = "Encrypted?";
            part.isEncrypted = true;
        }

        if (foundPartition) {
            // Проверяем, не пересекается ли с уже найденными разделами
            bool overlaps = false;
            for (const auto& existing : partitions) {
                if (offset >= existing.offset &&
                    offset < existing.offset + existing.size) {
                    overlaps = true;
                    qDebug() << "Skipping overlapping partition at offset" << offset;
                    break;
                }
            }

            if (!overlaps) {
                part.name = QString("Partition_%1").arg(partitions.size() + 1);
                part.offset = offset;
                part.filePath = filePath;
                part.size = determinePartitionSize(file, offset, fileSize);

                // Пытаемся определить имя
                QString guessedName = guessPartitionName(buffer, offset);
                if (!guessedName.isEmpty()) {
                    part.name = guessedName;
                }

                // Для userdata обычно большой размер
                if (part.name.toLower().contains("userdata") && part.size < 1024 * 1024 * 1024) {
                    part.size = fileSize - offset;
                }

                if (part.isValid()) {
                    partitions.append(part);
                    qDebug() << "Found partition:" << part.name
                             << "at offset 0x" << QString::number(offset, 16)
                             << "size:" << part.size / (1024*1024*1024.0) << "GB"
                             << "type:" << part.type;

                    // Перескакиваем вперед на размер раздела (минус шаг)
                    offset += qMax(part.size, SCAN_STEP) - SCAN_STEP;
                }
            }
        }
    }

    file.close();

    // Сортируем разделы по смещению
    std::sort(partitions.begin(), partitions.end(),
              [](const AndroidPartition& a, const AndroidPartition& b) {
                  return a.offset < b.offset;
              });

    // Попробуем определить имена разделов по порядку
    static const QStringList androidPartitionNames = {
        "boot", "recovery", "system", "vendor", "product",
        "odm", "system_ext", "metadata", "userdata", "cache",
        "persist", "modem", "bluetooth", "dsp", "efs"
    };

    for (int i = 0; i < qMin(partitions.size(), androidPartitionNames.size()); ++i) {
        if (partitions[i].name.startsWith("Partition_")) {
            partitions[i].name = androidPartitionNames[i];
        }
    }

    qDebug() << "\n=== PARTITION SUMMARY ===";
    for (int i = 0; i < partitions.size(); ++i) {
        const auto& part = partitions[i];
        qDebug() << QString("%1. %2").arg(i+1).arg(part.name);
        qDebug() << "   Offset: 0x" << QString::number(part.offset, 16);
        qDebug() << "   Size: " << part.humanReadableSize();
        qDebug() << "   Type: " << part.type;
        qDebug() << "   Encrypted: " << (part.isEncrypted ? "YES" : "NO");
    }

    qDebug() << "Total partitions found:" << partitions.size();
    return partitions;
}

qint64 PartitionParser::estimatePartitionSize(QFile& file, qint64 startOffset, qint64 maxSize)
{
    // Ищем следующую сигнатуру файловой системы
    const qint64 SEARCH_LIMIT = 100 * 1024 * 1024; // Ищем в пределах 100MB
    const qint64 SEARCH_STEP = 512;
    const qint64 READ_SIZE = 1024;

    qint64 searchEnd = qMin(startOffset + SEARCH_LIMIT, maxSize);

    for (qint64 offset = startOffset + SEARCH_STEP; offset < searchEnd; offset += SEARCH_STEP) {
        if (!file.seek(offset)) break;

        QByteArray buffer = file.read(READ_SIZE);
        if (buffer.size() < READ_SIZE) break;

        QString fsType = detectFilesystemType(buffer);
        if (fsType != "unknown") {
            // Нашли начало следующего раздела
            return offset - startOffset;
        }

        // Проверяем на LUKS заголовок
        if (buffer.startsWith("LUKS\xBA\xBE")) {
            return offset - startOffset;
        }
    }

    // Если не нашли следующую сигнатуру, возвращаем размер до конца файла
    return maxSize - startOffset;
}


QList<AndroidPartition> PartitionParser::parseMtkRawDump(const QString& filePath)
{
    QList<AndroidPartition> partitions;

    // Специфичные для MTK смещения
    QMap<QString, qint64> mtkOffsets = {
        {"preloader", 0x0},
        {"mbr", 0x0},
        {"ebr1", 0x8000},
        {"pmt", 0x40000},
        {"proinfo", 0x80000},
        {"nvram", 0xC0000},
        {"protect1", 0x100000},
        {"protect2", 0x200000},
        {"seccfg", 0x300000},
        {"uboot", 0x400000},
        {"bootimg", 0x500000},
        {"recovery", 0x600000},
        {"secro", 0x700000},
        {"misc", 0x800000},
        {"logo", 0x900000},
        {"expdb", 0xA00000},
        {"frp", 0xB00000},
        {"nvdata", 0xC00000},
        {"metadata", 0xD00000},
        {"system", 0xE00000},
        {"cache", 0x1E000000},
        {"userdata", 0x2E000000}
    };

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return partitions;
    }

    for (auto it = mtkOffsets.begin(); it != mtkOffsets.end(); ++it) {
        QString name = it.key();
        qint64 offset = it.value();

        if (offset >= file.size()) continue;

        if (file.seek(offset)) {
            QByteArray header = file.read(4096);

            if (!header.isEmpty()) {
                AndroidPartition part;
                part.name = name;
                part.offset = offset;
                part.filePath = filePath;
                part.type = "MTK";
                part.isEncrypted = false;

                // Для MTK часто размеры фиксированные
                if (name == "userdata") {
                    part.size = file.size() - offset;
                } else if (name == "system") {
                    part.size = 0x20000000; // 512MB типично
                } else {
                    part.size = 0x1000000; // 16MB по умолчанию
                }

                partitions.append(part);
            }
        }
    }

    file.close();
    return partitions;
}

// ============================================================================
// Вспомогательные функции для анализа данных
// ============================================================================

double PartitionParser::calculateEntropy(const QByteArray& data)
{
    if (data.isEmpty()) {
        return 0.0;
    }

    // Используем массив для подсчета частоты (256 возможных значений байта)
    int frequency[256] = {0};
    int totalBytes = data.size();

    // Подсчитываем частоту каждого байта
    for (int i = 0; i < totalBytes; ++i) {
        frequency[static_cast<unsigned char>(data[i])]++;
    }

    // Рассчитываем энтропию
    double entropy = 0.0;

    for (int i = 0; i < 256; ++i) {
        if (frequency[i] > 0) {
            double probability = static_cast<double>(frequency[i]) / totalBytes;
            entropy -= probability * log2(probability);
        }
    }

    // Энтропия в диапазоне 0-8 (для 8-битных данных)
    return entropy;
}

QString PartitionParser::bytesToHex(const QByteArray& data, int maxBytes)
{
    QString result;
    int bytesToShow = qMin(data.size(), maxBytes);

    for (int i = 0; i < bytesToShow; ++i) {
        result += QString("%1 ").arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0')).toUpper();

        // Разбиваем на строки по 16 байт
        if ((i + 1) % 16 == 0 && i != bytesToShow - 1) {
            result += "\n";
        } else if ((i + 1) % 8 == 0) {
            result += " ";
        }
    }

    if (data.size() > maxBytes) {
        result += QString(" ... (+%1 more bytes)").arg(data.size() - maxBytes);
    }

    return result;
}

QString PartitionParser::bytesToAscii(const QByteArray& data, int maxBytes)
{
    QString result;
    int bytesToShow = qMin(data.size(), maxBytes);

    for (int i = 0; i < bytesToShow; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);

        // Показываем печатаемые символы, остальные как точки
        if (c >= 32 && c <= 126) {
            result += QChar(c);
        } else if (c == 9 || c == 10 || c == 13) {
            result += ' ';
        } else {
            result += '.';
        }

        // Разбиваем на строки по 16 байт
        if ((i + 1) % 16 == 0 && i != bytesToShow - 1) {
            result += "\n";
        } else if ((i + 1) % 8 == 0) {
            result += " ";
        }
    }

    if (data.size() > maxBytes) {
        result += QString(" ... (+%1 more bytes)").arg(data.size() - maxBytes);
    }

    return result;
}

bool PartitionParser::isLikelyEncrypted(const QByteArray& data)
{
    if (data.size() < 1024) {
        return false;
    }

    // 1. Проверяем энтропию
    double entropy = calculateEntropy(data.left(4096));

    // Высокая энтропия (>7.5) характерна для зашифрованных данных
    if (entropy > 7.5) {
        return true;
    }

    // 2. Подсчитываем статистику
    int zeroCount = 0;
    int asciiCount = 0;
    int totalBytes = qMin(4096, data.size());

    for (int i = 0; i < totalBytes; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);

        if (c == 0) zeroCount++;
        if ((c >= 32 && c <= 126) || c == 9 || c == 10 || c == 13) asciiCount++;
    }

    double zeroRatio = static_cast<double>(zeroCount) / totalBytes;
    double asciiRatio = static_cast<double>(asciiCount) / totalBytes;

    // 3. Эвристики для определения шифрования:
    // - Мало ASCII символов (<5%)
    // - Не слишком много нулей (<80%)
    // - Не слишком мало нулей (>5%)
    if (asciiRatio < 0.05 && zeroRatio < 0.8 && zeroRatio > 0.05) {
        return true;
    }

    // 4. Проверяем наличие известных заголовков шифрования
    if (data.startsWith("LUKS\xBA\xBE")) {
        return true;
    }

    // 5. Проверяем на наличие паттернов (повторяющихся байтов)
    int patternCount = 0;
    for (int i = 0; i < totalBytes - 1; ++i) {
        if (data[i] == data[i + 1]) {
            patternCount++;
        }
    }

    double patternRatio = static_cast<double>(patternCount) / totalBytes;

    // Много повторяющихся байтов может указывать на несжатые или слабо зашифрованные данные
    if (patternRatio > 0.3) {
        return false; // Скорее не зашифровано
    }

    return false;
}

QString PartitionParser::detectFilesystemType(const QByteArray& header)
{
    if (header.size() < 1024) {
        return "unknown";
    }

    // 1. ext2/3/4 (0xEF53 at offset 0x438)
    if (header.size() >= 0x43A) {
        quint16 magic = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar*>(header.constData() + 0x438));
        if (magic == 0xEF53) {
            // Проверяем версию ext
            if (header.size() >= 0x458) {
                quint32 features = qFromLittleEndian<quint32>(
                    reinterpret_cast<const uchar*>(header.constData() + 0x458));
                if (features & 0x40) return "ext4";
                else if (features & 0x4) return "ext3";
                else return "ext2";
            }
            return "ext4"; // По умолчанию
        }
    }

    // 2. f2fs ("F2FS" at offset 0x400)
    if (header.size() >= 0x404 && header.mid(0x400, 4) == "F2FS") {
        return "f2fs";
    }

    // 3. FAT
    if (header.size() >= 0x5A) {
        QString fatStr = QString::fromLatin1(header.mid(0x36, 8).constData()).toUpper();
        if (fatStr.contains("FAT")) {
            return "fat";
        }
    }

    // 4. NTFS
    if (header.size() >= 0x0B && header.mid(0x03, 8) == "NTFS    ") {
        return "ntfs";
    }

    // 5. Linux swap
    if (header.size() >= 0x408) {
        QString swapStr = QString::fromLatin1(header.mid(0x400, 10).constData());
        if (swapStr.contains("SWAPSPACE") || swapStr.contains("SWAP-SPACE")) {
            return "swap";
        }
    }

    // 6. EROFS (Android 13+)
    if (header.size() >= 0x404 && header.mid(0x400, 4) == QByteArray("\xe2\xe1\xf5\xe0")) {
        return "erofs";
    }

    return "unknown";
}


qint64 PartitionParser::findNextSignature(QFile& file, qint64 startOffset, qint64 maxSize)
{
    const qint64 SEARCH_LIMIT = 200 * 1024 * 1024; // Ищем в пределах 200MB
    const qint64 SEARCH_STEP = 512 * 1024; // 512KB шаг
    const qint64 READ_SIZE = 8192;

    qint64 searchEnd = qMin(startOffset + SEARCH_LIMIT, maxSize);

    for (qint64 offset = startOffset + SEARCH_STEP; offset < searchEnd; offset += SEARCH_STEP) {
        if (!file.seek(offset)) break;

        QByteArray buffer = file.read(READ_SIZE);
        if (buffer.size() < 1024) break;

        // Проверяем на начало раздела
        if (detectFilesystemType(buffer) != "unknown" ||
            buffer.startsWith("LUKS\xBA\xBE") ||
            buffer.startsWith("ANDROID!") ||
            buffer.startsWith("VNDRBOOT") ||
            calculateEntropy(buffer.left(4096)) > 7.5) {
            return offset;
        }
    }

    // Если не нашли, возвращаем конец файла
    return maxSize;
}

QList<AndroidPartition> PartitionParser::extractInPriorityOrder(const QList<AndroidPartition>& partitions)
{
    // Приоритеты разделов для извлечения
    struct PriorityInfo {
        QString namePattern;
        int priority;
        QString description;
    };

    static const QVector<PriorityInfo> priorityList = {
        {"userdata", 1, "Пользовательские данные (самый важный)"},
        {"data", 1, "Данные пользователя"},
        {"metadata", 2, "Метаданные системы"},
        {"persist", 3, "Постоянные настройки"},
        {"nvdata", 3, "Данные NVRAM"},
        {"nvram", 3, "NVRAM"},
        {"frp", 4, "Factory Reset Protection"},
        {"protect", 4, "Защищенные разделы"},
        {"seccfg", 4, "Конфигурация безопасности"},
        {"super", 5, "Super раздел (системные образы)"},
        {"system", 6, "Системный раздел"},
        {"vendor", 6, "Вендорский раздел"},
        {"product", 6, "Продуктовый раздел"},
        {"boot", 7, "Загрузочный образ"},
        {"recovery", 7, "Recovery образ"},
        {"cache", 8, "Кэш"},
        {"misc", 9, "Разное"}
    };

    QList<QPair<int, AndroidPartition>> prioritized;

    for (const auto& partition : partitions) {
        QString lowerName = partition.name.toLower();
        int priority = 100; // Низкий приоритет по умолчанию

        // Находим приоритет по шаблону имени
        for (const auto& info : priorityList) {
            if (lowerName.contains(info.namePattern)) {
                priority = info.priority;
                break;
            }
        }

        prioritized.append(qMakePair(priority, partition));
    }

    // Сортируем по приоритету (меньшее число = выше приоритет)
    std::sort(prioritized.begin(), prioritized.end(),
              [](const QPair<int, AndroidPartition>& a,
                 const QPair<int, AndroidPartition>& b) {
                  return a.first < b.first;
              });

    // Преобразуем обратно в список
    QList<AndroidPartition> result;
    for (const auto& pair : prioritized) {
        result.append(pair.second);
    }

    return result;
}
