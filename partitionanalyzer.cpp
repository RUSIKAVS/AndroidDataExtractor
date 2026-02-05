#include "partitionanalyzer.h"
#include "superanalyzer.h"  // Добавьте эту строку
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QByteArray>
#include <QMap>
#include <tuple>

PartitionAnalyzer::PartitionAnalyzer()
{
}


bool PartitionAnalyzer::analyzePartitions(const QString &filePath)
{
    m_partitions.clear();
    m_lastError.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = "Не удалось открыть файл: " + filePath;
        qWarning() << m_lastError;
        return false;
    }

    // Получаем размер файла
    qint64 fileSize = file.size();
    qDebug() << "Анализ файла:" << filePath << "размер:" << fileSize;

    // Читаем первые 2MB для анализа (достаточно для GPT/MBR)
    QByteArray data = file.read(2 * 1024 * 1024);
    file.close();

    if (data.size() < 512) {
        m_lastError = "Файл слишком мал для анализа (меньше 512 байт)";
        qWarning() << m_lastError;
        return false;
    }

    // Проверяем сигнатуры
    bool hasGpt = false;
    bool hasMbr = false;

    // Проверка MBR сигнатуры (0x55AA в конце 512 байт)
    if (data.size() >= 512) {
        quint16 mbrSignature = *reinterpret_cast<const quint16*>(data.constData() + 510);
        if (mbrSignature == 0xAA55) { // little-endian
            hasMbr = true;
            qDebug() << "Найдена MBR сигнатура";
        }
    }

    // Проверка GPT сигнатуры ("EFI PART" по смещению 512)
    if (data.size() >= 1024) {
        QByteArray gptSignature = data.mid(512, 8);
        if (gptSignature == "EFI PART") {
            hasGpt = true;
            qDebug() << "Найдена GPT сигнатура";
        }
    }

    // Также проверяем по расширению файла
    QString extension = QFileInfo(filePath).suffix().toLower();
    bool isImageFile = extension == "img" || extension == "bin" || extension == "raw" ||
                       extension == "super" || extension == "sparseimg" ||
                       extension == "mbn" || extension == "sin" || extension == "dmp";

    qDebug() << "Результаты проверки: MBR =" << hasMbr << "GPT =" << hasGpt << "isImageFile =" << isImageFile;

    if (hasGpt) {
        m_partitionTableInfo = "GPT (GUID Partition Table)";
        qDebug() << "Парсинг GPT таблицы...";
        return parseGptPartitions(data, 512, fileSize);
    }
    else if (hasMbr) {
        m_partitionTableInfo = "MBR (Master Boot Record)";
        qDebug() << "Парсинг MBR таблицы...";
        return parseMbrPartitions(data, fileSize);
    }
    else if (isImageFile) {
        // Если это файл с расширением образа, но нет MBR/GPT, создаем тестовые разделы
        m_partitionTableInfo = "Unknown (тестовые данные)";
        qDebug() << "Нет MBR/GPT, создаю тестовые разделы для файла образа";
        createTestPartitions(fileSize);
        return true;
    }
    else {
        m_lastError = "Не найдена таблица разделов (ни MBR, ни GPT)";
        qWarning() << m_lastError;
        return false;
    }
}

bool PartitionAnalyzer::parseGptPartitions(const QByteArray &data, quint64 sectorSize, quint64 fileSize)
{
    try {
        // Парсинг GPT заголовка
        if (data.size() < static_cast<int>(sectorSize + 92)) {
            qWarning() << "Недостаточно данных для GPT парсинга";
            return false;
        }

        // Читаем количество записей разделов
        quint32 numPartitions = *reinterpret_cast<const quint32*>(data.constData() + sectorSize + 80);
        quint32 partitionEntrySize = *reinterpret_cast<const quint32*>(data.constData() + sectorSize + 84);
        quint64 partitionArrayStart = *reinterpret_cast<const quint64*>(data.constData() + sectorSize + 72);

        // Вычисляем смещение до таблицы разделов
        quint64 partitionTableOffset = partitionArrayStart * sectorSize;

        qDebug() << "GPT параметры:";
        qDebug() << "  разделов:" << numPartitions;
        qDebug() << "  размер записи:" << partitionEntrySize;
        qDebug() << "  смещение таблицы:" << partitionTableOffset;
        qDebug() << "  размер сектора:" << sectorSize;

        // Ограничиваем количество разделов для безопасности
        if (numPartitions > 128) {
            qWarning() << "Слишком много разделов в GPT:" << numPartitions;
            numPartitions = 128;
        }

        // Вычисляем размер таблицы разделов
        quint64 partitionTableSize = numPartitions * partitionEntrySize;

        qDebug() << "Размер таблицы разделов:" << partitionTableSize << "байт";

        // Для реализации реального парсинга нужно читать таблицу разделов из файла
        // Пока создаем реалистичные тестовые разделы
        createRealisticTestPartitions(fileSize);

        return true;

    } catch (const std::exception& e) {
        qWarning() << "Ошибка при парсинге GPT:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "Неизвестная ошибка при парсинге GPT";
        return false;
    }
}

bool PartitionAnalyzer::parseMbrPartitions(const QByteArray &data, quint64 fileSize)
{
    try {
        qDebug() << "Начало парсинга MBR, размер данных:" << data.size() << "размер файла:" << fileSize;

        // MBR имеет 4 записи разделов, начиная со смещения 446
        const int mbrPartitionOffset = 446;
        const int partitionEntrySize = 16;

        // Проверяем MBR сигнатуру еще раз
        if (data.size() < 512) {
            qWarning() << "Недостаточно данных для MBR парсинга";
            return false;
        }

        quint16 mbrSignature = *reinterpret_cast<const quint16*>(data.constData() + 510);
        if (mbrSignature != 0xAA55) {
            qWarning() << "Неверная MBR сигнатура:" << QString::number(mbrSignature, 16);
            return false;
        }

        qDebug() << "MBR сигнатура верная: 0x" << QString::number(mbrSignature, 16);

        bool foundPartitions = false;

        for (int i = 0; i < 4; i++) {
            int offset = mbrPartitionOffset + i * partitionEntrySize;

            if (offset + partitionEntrySize > data.size()) {
                qWarning() << "Выход за пределы данных при чтении MBR раздела" << i;
                break;
            }

            // Читаем данные раздела
            const unsigned char* partitionData = reinterpret_cast<const unsigned char*>(data.constData() + offset);

            // Проверяем, активен ли раздел (0x80 = активный, 0x00 = неактивный)
            quint8 status = partitionData[0];
            quint8 typeCode = partitionData[4];

            qDebug() << "MBR раздел" << i << "status:" << QString::number(status, 16)
                     << "type:" << QString::number(typeCode, 16);

            // Пропускаем пустые записи (статус 0x00 и тип 0x00)
            if (status == 0x00 && typeCode == 0x00) {
                qDebug() << "  Пустая запись, пропускаем";
                continue;
            }

            // Если тип раздела 0x00 - пустая запись (даже если статус не 0x00)
            if (typeCode == 0x00) {
                qDebug() << "  Тип раздела 0x00, пропускаем";
                continue;
            }

            // Типы разделов, которые мы пропускаем (служебные)
            if (typeCode == 0x05 || typeCode == 0x0F || typeCode == 0x85) {
                qDebug() << "  Расширенный раздел (тип 0x" << QString::number(typeCode, 16) << "), пропускаем";
                continue;
            }

            PartitionInfo partition;
            partition.name = QString("Partition %1").arg(i + 1);
            partition.type = getPartitionType(typeCode);

            // Читаем смещение и размер из MBR (в секторах, LBA формат)
            quint32 startSector = *reinterpret_cast<const quint32*>(partitionData + 8);
            quint32 sectorCount = *reinterpret_cast<const quint32*>(partitionData + 12);

            // Конвертируем в байты (предполагаем размер сектора 512 байт)
            partition.offset = static_cast<quint64>(startSector) * 512;
            partition.size = static_cast<quint64>(sectorCount) * 512;

            // Проверяем, что раздел не выходит за пределы файла
            if (partition.offset >= fileSize) {
                qWarning() << "  Раздел" << partition.name << "начинается за пределами файла";
                continue;
            }

            if (partition.offset + partition.size > fileSize) {
                qWarning() << "  Раздел" << partition.name << "частично за пределами файла";
                partition.size = fileSize - partition.offset;
            }

            // Генерируем GUID для MBR раздела на основе типа и смещения
            partition.guid = QString("MBR-%1-%2-%3")
                                 .arg(typeCode, 2, 16, QLatin1Char('0'))
                                 .arg(i)
                                 .arg(startSector, 8, 16, QLatin1Char('0'))
                                 .toUpper();

            // Проверяем, что размер раздела не нулевой
            if (partition.size == 0) {
                qWarning() << "  Раздел" << partition.name << "имеет нулевой размер";
                continue;
            }

            m_partitions.push_back(partition);
            foundPartitions = true;

            qDebug() << "  Найден MBR раздел" << i << ":";
            qDebug() << "    имя:" << partition.name;
            qDebug() << "    тип:" << partition.type << "(код: 0x" << QString::number(typeCode, 16) << ")";
            qDebug() << "    смещение:" << partition.offset << "байт";
            qDebug() << "    размер:" << partition.size << "байт";
            qDebug() << "    начальный сектор:" << startSector;
            qDebug() << "    количество секторов:" << sectorCount;
            qDebug() << "    GUID:" << partition.guid;
        }

        if (!foundPartitions) {
            qDebug() << "Не найдено ни одного валидного MBR раздела";
            // Проверяем, не является ли это супер разделом
            QFileInfo fileInfo(m_lastFilePath);
            if (SuperAnalyzer::isSuperImage(fileInfo.absoluteFilePath())) {
                qDebug() << "Файл является SUPER разделом";
                PartitionInfo superPartition;
                superPartition.name = "super";
                superPartition.guid = "{E6A98E58-E8E4-4C6E-B078-8B3A7A7B5B9E}";
                superPartition.size = fileSize;
                superPartition.type = "Android Dynamic";
                superPartition.offset = 0;
                m_partitions.push_back(superPartition);
                return true;
            }
            return false;
        }

        qDebug() << "Найдено MBR разделов:" << m_partitions.size();
        return true;

    } catch (const std::exception& e) {
        qWarning() << "Ошибка при парсинге MBR:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "Неизвестная ошибка при парсинге MBR";
        return false;
    }
}


std::vector<PartitionInfo> PartitionAnalyzer::getPartitions() const
{
    return m_partitions;
}

bool PartitionAnalyzer::isDiskImage(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.read(1024);
    file.close();

    if (data.size() < 512) {
        return false;
    }

    // Проверка MBR
    quint16 mbrSignature = *reinterpret_cast<const quint16*>(data.constData() + 510);
    if (mbrSignature == 0xAA55) {
        return true;
    }

    // Проверка GPT
    if (data.size() >= 1024) {
        QByteArray gptSignature = data.mid(512, 8);
        if (gptSignature == "EFI PART") {
            return true;
        }
    }

    // Проверка по расширению файла
    QString extension = QFileInfo(filePath).suffix().toLower();
    return extension == "img" || extension == "bin" || extension == "raw" ||
           extension == "super" || extension == "sparseimg";
}

QString PartitionAnalyzer::getPartitionTableInfo() const
{
    return m_partitionTableInfo;
}

QString PartitionAnalyzer::readGuid(const QByteArray &data, int offset) const
{
    if (data.size() < offset + 16) {
        return "{INVALID_GUID}";
    }

    const unsigned char *guid = reinterpret_cast<const unsigned char*>(data.constData() + offset);

    // Форматирование GUID согласно стандарту
    return QString("{%1%2%3%4-%5%6-%7%8-%9%10-%11%12%13%14%15%16}")
        .arg(guid[3], 2, 16, QLatin1Char('0'))
        .arg(guid[2], 2, 16, QLatin1Char('0'))
        .arg(guid[1], 2, 16, QLatin1Char('0'))
        .arg(guid[0], 2, 16, QLatin1Char('0'))
        .arg(guid[5], 2, 16, QLatin1Char('0'))
        .arg(guid[4], 2, 16, QLatin1Char('0'))
        .arg(guid[7], 2, 16, QLatin1Char('0'))
        .arg(guid[6], 2, 16, QLatin1Char('0'))
        .arg(guid[8], 2, 16, QLatin1Char('0'))
        .arg(guid[9], 2, 16, QLatin1Char('0'))
        .arg(guid[10], 2, 16, QLatin1Char('0'))
        .arg(guid[11], 2, 16, QLatin1Char('0'))
        .arg(guid[12], 2, 16, QLatin1Char('0'))
        .arg(guid[13], 2, 16, QLatin1Char('0'))
        .arg(guid[14], 2, 16, QLatin1Char('0'))
        .arg(guid[15], 2, 16, QLatin1Char('0'))
        .toUpper();
}

QString PartitionAnalyzer::getPartitionType(quint8 typeCode) const
{
    static const QMap<quint8, QString> typeMap = {
        {0x00, "Empty"},
        {0x01, "FAT12"},
        {0x04, "FAT16 <32MB"},
        {0x05, "Extended"},
        {0x06, "FAT16"},
        {0x07, "NTFS/HPFS"},
        {0x0B, "FAT32"},
        {0x0C, "FAT32 (LBA)"},
        {0x0E, "FAT16 (LBA)"},
        {0x0F, "Extended (LBA)"},
        {0x11, "Hidden FAT12"},
        {0x14, "Hidden FAT16 <32MB"},
        {0x16, "Hidden FAT16"},
        {0x17, "Hidden NTFS"},
        {0x1B, "Hidden FAT32"},
        {0x1C, "Hidden FAT32 (LBA)"},
        {0x1E, "Hidden FAT16 (LBA)"},
        {0x82, "Linux Swap"},
        {0x83, "Linux"},
        {0x85, "Linux Extended"},
        {0x86, "NTFS Volume Set"},
        {0x87, "NTFS Volume Set"},
        {0xA5, "FreeBSD"},
        {0xA6, "OpenBSD"},
        {0xA8, "Mac OSX"},
        {0xA9, "NetBSD"},
        {0xAB, "Mac OS X Boot"},
        {0xAF, "Mac OS X HFS+"},
        {0xB7, "BSDI"},
        {0xB8, "BSDI Swap"},
        {0xEE, "GPT Protective"},
        {0xEF, "EFI System"},
        {0xFB, "VMware VMFS"},
        {0xFC, "VMware VMKCORE"},
        {0xFD, "Linux RAID"}
    };

    if (typeMap.contains(typeCode)) {
        return typeMap[typeCode];
    }

    return QString("Unknown (0x%1)").arg(typeCode, 2, 16, QLatin1Char('0'));
}

QString PartitionAnalyzer::getPartitionTypeFromGuid(const QString &guid) const
{
    static const QMap<QString, QString> guidTypeMap = {
        // Стандартные GUID
        {"C12A7328-F81F-11D2-BA4B-00A0C93EC93B", "EFI System"},
        {"024DEE41-33E7-11D3-9D69-0008C781F39F", "MBR"},
        {"E3C9E316-0B5C-4DB8-817D-F92DF00215AE", "Microsoft Reserved"},
        {"EBD0A0A2-B9E5-4433-87C0-68B6B72699C7", "Microsoft Basic Data"},

        // Android GUID
        {"19A710A2-B3CA-11E4-B026-10604B889DCF", "Android Bootloader"},
        {"193D1EA4-B3CA-11E4-B075-10604B889DCF", "Android Boot"},
        {"A19EA859-4D6F-7442-8235-686F6C746572", "Android System"},
        {"C5A0AEEC-13EA-11E5-A1B1-001E67CA0C3C", "Android Vendor"},

        // Linux GUID
        {"0FC63DAF-8483-4772-8E79-3D69D8477DE4", "Linux Filesystem"},
        {"0657FD6D-A4AB-43C4-84E5-0933C84B4F4F", "Linux Swap"},
        {"E6D6D379-F507-44C2-A23C-238F2A3DF928", "Linux LVM"},

        // Dynamic Partitions
        {"E6A98E58-E8E4-4C6E-B078-8B3A7A7B5B9E", "Android Dynamic"}
    };

    QString normalizedGuid = guid.toUpper();
    for (auto it = guidTypeMap.constBegin(); it != guidTypeMap.constEnd(); ++it) {
        if (normalizedGuid.contains(it.key())) {
            return it.value();
        }
    }

    return "Unknown GUID";
}

void PartitionAnalyzer::createTestPartitions()
{
    m_partitions.clear();

    // Стандартные разделы для Android образов
    std::vector<PartitionInfo> testPartitions;

    testPartitions.push_back(PartitionInfo(
        "boot",
        "{19A710A2-B3CA-11E4-B026-10604B889DCF}",
        67108864,  // 64MB
        "Android Boot",
        0
        ));

    testPartitions.push_back(PartitionInfo(
        "dtbo",
        "{193D1EA4-B3CA-11E4-B075-10604B889DCF}",
        8388608,   // 8MB
        "Android DTBO",
        67108864
        ));

    testPartitions.push_back(PartitionInfo(
        "recovery",
        "{A19EA859-4D6F-7442-8235-686F6C746572}",
        67108864,  // 64MB
        "Android Recovery",
        75497472
        ));

    testPartitions.push_back(PartitionInfo(
        "system",
        "{C5A0AEEC-13EA-11E5-A1B1-001E67CA0C3C}",
        2147483648,  // 2GB
        "Android System",
        142606336
        ));

    testPartitions.push_back(PartitionInfo(
        "vendor",
        "{BD59408B-4514-490D-BF12-9878D963A378}",
        1073741824,  // 1GB
        "Android Vendor",
        2290089984
        ));

    testPartitions.push_back(PartitionInfo(
        "product",
        "{0FC63DAF-8483-4772-8E79-3D69D8477DE4}",
        536870912,  // 512MB
        "Android Product",
        3363831808
        ));

    testPartitions.push_back(PartitionInfo(
        "super",
        "{E6A98E58-E8E4-4C6E-B078-8B3A7A7B5B9E}",
        4294967296,  // 4GB
        "Android Dynamic",
        3900702720
        ));

    testPartitions.push_back(PartitionInfo(
        "userdata",
        "{0657FD6D-A4AB-43C4-84E5-0933C84B4F4F}",
        8589934592,  // 8GB
        "Android Userdata",
        8195670016
        ));

    m_partitions = testPartitions;
    m_partitionTableInfo = "Test Data (GPT-like)";

    qDebug() << "Создано" << m_partitions.size() << "тестовых разделов";
}

// Создание реалистичных тестовых разделов (используется при отсутствии реальной таблицы разделов)
void PartitionAnalyzer::createRealisticTestPartitions(quint64 fileSize)
{
    m_partitions.clear();

    if (fileSize == 0) {
        fileSize = 8ULL * 1024 * 1024 * 1024; // 8GB по умолчанию
    }

    // Типичные разделы для Android устройств (в порядке смещения)
    struct AndroidPartition {
        QString name;
        QString guid;
        double sizeMB;  // Размер в MB
        QString type;
    };

    std::vector<AndroidPartition> typicalPartitions = {
        {"boot", "{19A710A2-B3CA-11E4-B026-10604B889DCF}", 64.0, "Android Boot"},
        {"dtbo", "{193D1EA4-B3CA-11E4-B075-10604B889DCF}", 8.0, "Android DTBO"},
        {"recovery", "{A19EA859-4D6F-7442-8235-686F6C746572}", 64.0, "Android Recovery"},
        {"system", "{C5A0AEEC-13EA-11E5-A1B1-001E67CA0C3C}", 2048.0, "Android System"},      // 2GB
        {"vendor", "{BD59408B-4514-490D-BF12-9878D963A378}", 1024.0, "Android Vendor"},      // 1GB
        {"product", "{0FC63DAF-8483-4772-8E79-3D69D8477DE4}", 512.0, "Android Product"},     // 512MB
        {"super", "{E6A98E58-E8E4-4C6E-B078-8B3A7A7B5B9E}", 4096.0, "Android Dynamic"},     // 4GB
        {"userdata", "{0657FD6D-A4AB-43C4-84E5-0933C84B4F4F}", 0.0, "Android Userdata"},     // Оставшееся место
    };

    quint64 currentOffset = 0;
    const quint64 MB = 1024 * 1024;
    quint64 remainingSize = fileSize;

    for (const auto& part : typicalPartitions) {
        quint64 size = (part.sizeMB > 0) ? static_cast<quint64>(part.sizeMB * MB) : 0;

        // Для userdata берем все оставшееся место
        if (part.name == "userdata") {
            size = remainingSize;
        }

        // Проверяем, чтобы раздел не выходил за пределы файла
        if (size > remainingSize || size == 0) {
            size = remainingSize;
        }

        if (size < 1 * MB && part.name != "userdata") {
            // Пропускаем маленькие разделы в конце файла
            break;
        }

        PartitionInfo partition;
        partition.name = part.name;
        partition.guid = part.guid;
        partition.size = size;
        partition.type = part.type;
        partition.offset = currentOffset;

        m_partitions.push_back(partition);
        currentOffset += size;
        remainingSize -= size;

        qDebug() << "Создан тестовый раздел:" << part.name
                 << "size:" << size << "байт (" << (size / MB) << "MB)"
                 << "offset:" << partition.offset;

        if (remainingSize <= 0) {
            break;
        }
    }

    m_partitionTableInfo = "GPT (Тестовые данные)";
    qDebug() << "Создано" << m_partitions.size() << "реалистичных тестовых разделов";
}
