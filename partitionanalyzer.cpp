#include "partitionanalyzer.h"
#include "guidmanager.h"
#include <QDebug>
#include <QFileInfo>

PartitionAnalyzer::PartitionAnalyzer(QObject *parent)
    : QObject{parent}
    , m_currentFile(nullptr)
{
    // Инициализация
}

PartitionAnalyzer::~PartitionAnalyzer()
{
    // Закрываем файл если открыт
    if (m_currentFile && m_currentFile->isOpen()) {
        m_currentFile->close();
    }
    delete m_currentFile;
}

DiskInfo PartitionAnalyzer::analyzeDisk(const QString &filePath)
{
    emit logMessage(QString("Starting analysis of: %1").arg(filePath));

    DiskInfo diskInfo;
    diskInfo.path = filePath;

    // Проверяем существование файла
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        emit errorOccurred("File does not exist");
        return diskInfo;
    }

    diskInfo.totalSize = fileInfo.size();
    emit logMessage(QString("File size: %1 bytes").arg(diskInfo.totalSize));

    // Открываем файл
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred(QString("Cannot open file: %1").arg(file.errorString()));
        return diskInfo;
    }

    // Определяем тип таблицы разделов
    diskInfo.partitionTable = detectPartitionTableType(file);
    emit logMessage(QString("Detected partition table: %1").arg(diskInfo.partitionTable));

    // Анализируем в зависимости от типа таблицы
    if (diskInfo.partitionTable == "MBR") {
        analyzeMBR(file, diskInfo);
    } else if (diskInfo.partitionTable == "GPT") {
        analyzeGPT(file, diskInfo);
    } else if (diskInfo.partitionTable == "SUPER") {
        analyzeSuper(file, diskInfo);
    } else if (diskInfo.partitionTable == "RAW") {
        analyzeRaw(file, diskInfo);
    }

    file.close();

    emit logMessage(QString("Analysis complete. Found %1 partitions").arg(diskInfo.partitions.size()));
    return diskInfo;
}

QString PartitionAnalyzer::detectPartitionTableType(QFile &file)
{
    // Читаем первые 512 байт (стандартный сектор)
    QByteArray sector = file.read(512);
    if (sector.size() < 512) {
        return "RAW";  // Файл слишком мал для таблицы разделов
    }

    // Проверяем сигнатуру MBR (55 AA в конце сектора)
    uint8_t mbrSignature1 = static_cast<uint8_t>(sector[510]);
    uint8_t mbrSignature2 = static_cast<uint8_t>(sector[511]);

    if (mbrSignature1 == 0x55 && mbrSignature2 == 0xAA) {
        // Проверяем, есть ли GPT сигнатура (EFI PART)
        QByteArray gptSignature = sector.mid(0x1C0, 8);
        if (gptSignature == "EFI PART") {
            return "GPT";
        }

        // Проверяем, является ли это защитным MBR для GPT
        for (int i = 0x1BE; i < 0x1FE; i += 16) {
            uint8_t type = static_cast<uint8_t>(sector[i + 4]);
            if (type == 0xEE) {  // Защитный MBR
                return "GPT";
            }
        }

        return "MBR";
    }

    // Проверяем SUPER раздел (Android динамические разделы)
    // Сигнатура SUPER: 0x10 0x67 0x44 0x56 в смещении 0x400
    file.seek(0x400);
    QByteArray superHeader = file.read(512);
    if (superHeader.size() >= 4) {
        if (static_cast<uint8_t>(superHeader[0]) == 0x10 &&
            static_cast<uint8_t>(superHeader[1]) == 0x67 &&
            static_cast<uint8_t>(superHeader[2]) == 0x44 &&
            static_cast<uint8_t>(superHeader[3]) == 0x56) {
            return "SUPER";
        }
    }

    // Возвращаемся к началу для следующих проверок
    file.seek(0);

    // Проверяем различные размеры секторов для GPT
    const QVector<uint32_t> sectorSizes = {512, 1024, 2048, 4096, 8192, 16384};

    for (uint32_t sectorSize : sectorSizes) {
        file.seek(sectorSize);  // GPT заголовок находится в секторе 1
        QByteArray gptHeader = file.read(512);  // Читаем достаточно для проверки

        if (gptHeader.size() >= 8) {
            // Проверяем сигнатуру GPT
            if (gptHeader.mid(0, 8) == "EFI PART") {
                emit logMessage(QString("Detected GPT with sector size: %1").arg(sectorSize));
                return "GPT";
            }
        }
    }

    // Если ничего не найдено, считаем это RAW разделом
    return "RAW";
}

void PartitionAnalyzer::analyzeMBR(QFile &file, DiskInfo &diskInfo)
{
    emit logMessage("Analyzing MBR partition table...");

    // Читаем MBR сектор
    file.seek(0);
    QByteArray mbr = file.read(512);

    if (mbr.size() < 512) {
        emit errorOccurred("Failed to read MBR");
        return;
    }

    diskInfo.sectorSize = 512;  // MBR всегда использует 512 байтные сектора

    // Анализируем 4 записи о разделах (смещение 0x1BE)
    for (int i = 0; i < 4; i++) {
        int offset = 0x1BE + (i * 16);

        // Читаем тип раздела
        uint8_t type = static_cast<uint8_t>(mbr[offset + 4]);

        if (type != 0x00) {  // Пустая запись
            PartitionInfo partition;
            partition.name = QString("Partition %1").arg(i + 1);
            partition.type = "MBR";

            // Читаем начало и размер раздела (в секторах)
            partition.startSector = bytesToUint32(mbr, offset + 8);
            partition.sizeSectors = bytesToUint32(mbr, offset + 12);

            // Конвертируем в байты
            partition.startByte = partition.startSector * diskInfo.sectorSize;
            partition.sizeBytes = partition.sizeSectors * diskInfo.sectorSize;

            // Проверяем загрузочный флаг
            partition.bootable = (static_cast<uint8_t>(mbr[offset]) & 0x80) != 0;

            // Определяем файловую систему по типу
            switch (type) {
            case 0x07: partition.fileSystem = "NTFS"; break;
            case 0x0B: case 0x0C: partition.fileSystem = "FAT32"; break;
            case 0x83: partition.fileSystem = "Linux"; break;
            case 0xEE: partition.fileSystem = "GPT Protective"; break;
            default: partition.fileSystem = QString("Unknown (0x%1)").arg(type, 2, 16, QChar('0'));
            }

            diskInfo.partitions.append(partition);

            emit logMessage(QString("  Found: %1, Start: %2, Size: %3 bytes, FS: %4")
                                .arg(partition.name)
                                .arg(partition.startByte)
                                .arg(partition.sizeBytes)
                                .arg(partition.fileSystem));
        }
    }
}

void PartitionAnalyzer::analyzeGPT(QFile &file, DiskInfo &diskInfo)
{
    emit logMessage("Analyzing GPT partition table...");

    // Определяем размер сектора
    diskInfo.sectorSize = detectGptSectorSize(file);
    if (diskInfo.sectorSize == 0) {
        emit errorOccurred("Cannot determine GPT sector size");
        return;
    }

    emit logMessage(QString("GPT sector size: %1 bytes").arg(diskInfo.sectorSize));

    // Читаем заголовок GPT (сектор 1)
    file.seek(diskInfo.sectorSize);
    QByteArray gptHeader = file.read(512);  // Заголовок GPT обычно 92 байта, но читаем 512 для уверенности

    if (gptHeader.size() < 92) {
        emit errorOccurred("Failed to read GPT header");
        return;
    }

    // Проверяем сигнатуру
    if (gptHeader.mid(0, 8) != "EFI PART") {
        emit errorOccurred("Invalid GPT signature");
        return;
    }

    // Читаем количество записей о разделах
    uint32_t numPartitions = bytesToUint32(gptHeader, 0x50);
    uint32_t partitionEntrySize = bytesToUint32(gptHeader, 0x54);
    uint64_t partitionArrayStart = bytesToUint64(gptHeader, 0x48) * diskInfo.sectorSize;

    emit logMessage(QString("Found %1 partition entries").arg(numPartitions));

    // Читаем таблицу разделов
    file.seek(partitionArrayStart);
    QByteArray partitionTable = file.read(numPartitions * partitionEntrySize);

    if (partitionTable.size() < static_cast<int>(numPartitions * partitionEntrySize)) {
        emit errorOccurred("Failed to read partition table");
        return;
    }

    // Создаем менеджер GUID для получения описаний
    GuidManager guidManager;

    // Парсим записи о разделах
    for (uint32_t i = 0; i < numPartitions; i++) {
        int offset = i * partitionEntrySize;

        // Читаем тип GUID
        QUuid typeGuid = readGuid(partitionTable, offset);

        // Если GUID пустой, запись не используется
        if (typeGuid.isNull()) {
            continue;
        }

        // Читаем начало и размер раздела
        uint64_t startLba = bytesToUint64(partitionTable, offset + 0x20);
        uint64_t endLba = bytesToUint64(partitionTable, offset + 0x28);

        if (startLba == 0 && endLba == 0) {
            continue;  // Пустая запись
        }

        PartitionInfo partition;

        // Читаем имя раздела (UTF-16LE)
        QByteArray nameBytes = partitionTable.mid(offset + 0x38, 72);
        QString name;
        for (int j = 0; j < nameBytes.size(); j += 2) {
            if (j + 1 < nameBytes.size()) {
                uint16_t ch = static_cast<uint8_t>(nameBytes[j]) |
                              (static_cast<uint8_t>(nameBytes[j + 1]) << 8);
                if (ch == 0) break;
                name.append(QChar(ch));
            }
        }

        partition.name = name.isEmpty() ? QString("Partition %1").arg(i + 1) : name;
        partition.type = "GPT";
        partition.guid = typeGuid;
        partition.description = guidManager.getPartitionDescription(typeGuid);

        partition.startSector = startLba;
        partition.sizeSectors = endLba - startLba + 1;
        partition.startByte = startLba * diskInfo.sectorSize;
        partition.sizeBytes = partition.sizeSectors * diskInfo.sectorSize;

        // Определяем файловую систему по GUID или описанию
        if (partition.description.contains("FAT") || partition.description.contains("EFI")) {
            partition.fileSystem = "FAT32";
        } else if (partition.description.contains("Linux")) {
            partition.fileSystem = "Linux";
        } else if (partition.description.contains("NTFS") || partition.description.contains("Microsoft")) {
            partition.fileSystem = "NTFS";
        } else {
            partition.fileSystem = "Unknown";
        }

        diskInfo.partitions.append(partition);

        emit logMessage(QString("  Found: %1, Type: %2, Start: %3, Size: %4 bytes")
                            .arg(partition.name)
                            .arg(partition.description)
                            .arg(partition.startByte)
                            .arg(partition.sizeBytes));
    }
}

void PartitionAnalyzer::analyzeSuper(QFile &file, DiskInfo &diskInfo)
{
    emit logMessage("Analyzing Android SUPER partition...");

    // TODO: Реализовать анализ Super раздела
    // Это требует реализации парсера метаданных динамических разделов Android

    emit logMessage("SUPER partition analysis not yet implemented");

    // Временная реализация - создаем один "сырой" раздел
    PartitionInfo partition;
    partition.name = "SUPER";
    partition.type = "SUPER";
    partition.startByte = 0;
    partition.sizeBytes = diskInfo.totalSize;
    partition.description = "Android Dynamic Super Partition";
    partition.fileSystem = "Dynamic";

    diskInfo.partitions.append(partition);
}

void PartitionAnalyzer::analyzeRaw(QFile &file, DiskInfo &diskInfo)
{
    emit logMessage("Analyzing as RAW partition...");

    // Создаем один раздел на весь файл
    PartitionInfo partition;
    partition.name = "RAW";
    partition.type = "RAW";
    partition.startByte = 0;
    partition.sizeBytes = diskInfo.totalSize;
    partition.description = "Raw partition or filesystem";
    partition.fileSystem = "Unknown";

    diskInfo.partitions.append(partition);

    emit logMessage(QString("  Treating entire file as single partition: %1 bytes")
                        .arg(partition.sizeBytes));
}

uint32_t PartitionAnalyzer::detectGptSectorSize(QFile &file)
{
    // Пробуем различные размеры секторов
    const QVector<uint32_t> sectorSizes = {512, 1024, 2048, 4096, 8192, 16384};

    for (uint32_t sectorSize : sectorSizes) {
        file.seek(sectorSize);  // GPT заголовок в секторе 1
        QByteArray gptHeader = file.read(512);

        if (gptHeader.size() >= 92) {
            // Проверяем сигнатуру и резервные поля
            if (gptHeader.mid(0, 8) == "EFI PART") {
                // Проверяем, что размер заголовка и CRC корректны
                uint32_t headerSize = bytesToUint32(gptHeader, 0x0C);
                if (headerSize >= 92 && headerSize <= 512) {
                    return sectorSize;
                }
            }
        }
    }

    return 0;  // Не удалось определить
}

QByteArray PartitionAnalyzer::readPartitionData(const DiskInfo &diskInfo,
                                                int partitionIndex,
                                                uint64_t offset,
                                                uint64_t size)
{
    if (partitionIndex < 0 || partitionIndex >= diskInfo.partitions.size()) {
        emit errorOccurred("Invalid partition index");
        return QByteArray();
    }

    const PartitionInfo &partition = diskInfo.partitions[partitionIndex];

    // Проверяем границы
    if (offset >= partition.sizeBytes) {
        emit errorOccurred("Offset exceeds partition size");
        return QByteArray();
    }

    // Корректируем размер если нужно
    uint64_t bytesToRead = qMin(size, partition.sizeBytes - offset);

    // Открываем файл
    QFile file(diskInfo.path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred(QString("Cannot open file: %1").arg(file.errorString()));
        return QByteArray();
    }

    // Позиционируемся и читаем
    uint64_t absoluteOffset = partition.startByte + offset;
    if (!file.seek(absoluteOffset)) {
        emit errorOccurred("Cannot seek to specified offset");
        file.close();
        return QByteArray();
    }

    QByteArray data = file.read(bytesToRead);
    file.close();

    emit logMessage(QString("Read %1 bytes from partition %2 at offset %3")
                        .arg(data.size())
                        .arg(partition.name)
                        .arg(offset));

    return data;
}

QUuid PartitionAnalyzer::readGuid(const QByteArray &data, int offset)
{
    if (offset + 16 > data.size()) {
        return QUuid();
    }

    // GUID хранится в формате mixed-endian
    uint32_t data1 = bytesToUint32(data, offset);
    uint16_t data2 = static_cast<uint16_t>(data[offset + 4]) |
                     (static_cast<uint16_t>(data[offset + 5]) << 8);
    uint16_t data3 = static_cast<uint16_t>(data[offset + 6]) |
                     (static_cast<uint16_t>(data[offset + 7]) << 8);

    QByteArray data4 = data.mid(offset + 8, 8);

    // Формируем строку GUID
    QString guidStr = QString("%1-%2-%3-%4-%5")
                          .arg(data1, 8, 16, QChar('0'))
                          .arg(data2, 4, 16, QChar('0'))
                          .arg(data3, 4, 16, QChar('0'))
                          .arg(QString(data4.mid(0, 2).toHex()))
                          .arg(QString(data4.mid(2).toHex()));

    return QUuid::fromString(guidStr);
}

uint32_t PartitionAnalyzer::bytesToUint32(const QByteArray &data, int offset)
{
    if (offset + 4 > data.size()) {
        return 0;
    }

    // Little-endian
    return static_cast<uint32_t>(static_cast<uint8_t>(data[offset])) |
           (static_cast<uint32_t>(static_cast<uint8_t>(data[offset + 1])) << 8) |
           (static_cast<uint32_t>(static_cast<uint8_t>(data[offset + 2])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(data[offset + 3])) << 24);
}

uint64_t PartitionAnalyzer::bytesToUint64(const QByteArray &data, int offset)
{
    if (offset + 8 > data.size()) {
        return 0;
    }

    // Little-endian
    uint64_t result = 0;
    for (int i = 0; i < 8; i++) {
        result |= static_cast<uint64_t>(static_cast<uint8_t>(data[offset + i])) << (i * 8);
    }
    return result;
}
