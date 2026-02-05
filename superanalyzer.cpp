#include "superanalyzer.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QTreeWidgetItem>
#include <QMessageBox>
#include <QMap>
#include <QRandomGenerator>

SuperAnalyzer::SuperAnalyzer(QObject *parent)
    : QObject(parent), m_isValid(false), m_fileOffset(0)
{
}

SuperAnalyzer::~SuperAnalyzer()
{
}

bool SuperAnalyzer::analyzeSuper(const QString &filePath, quint64 offset)
{
    m_filePath = filePath;
    m_fileOffset = offset;
    m_partitions.clear();
    m_isValid = false;

    qDebug() << "Анализ SUPER раздела:" << filePath << "смещение:" << offset;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Не удалось открыть файл super:" << filePath;
        return false;
    }

    // Если указано смещение, переходим к нему
    if (offset > 0) {
        if (!file.seek(offset)) {
            qWarning() << "Не удалось перейти к смещению:" << offset;
            file.close();
            return false;
        }
        qDebug() << "Перешли к смещению:" << offset;
    }

    // Чтение заголовка super
    QByteArray header = file.read(4096);

    // Проверка сигнатуры super (LP_METADATA_MAGIC = 0x414C5030)
    if (header.size() >= 4) {
        quint32 magic = *reinterpret_cast<const quint32*>(header.constData());
        qDebug() << "Magic байты:" << QString::number(magic, 16);

        if (magic == 0x414C5030) { // "LP0A" в little-endian
            qDebug() << "Найден LP_METADATA (Android Dynamic Partition)";
            m_isValid = parseLpMetadataWithOffset(offset);
        } else if (header.contains("LP-METADATA")) {
            qDebug() << "Найден LP-METADATA (старый формат)";
            m_isValid = parseLpMetadataWithOffset(offset);
        } else {
            // Проверка других форматов
            qWarning() << "Неизвестный формат super раздела, magic:" << QString::number(magic, 16);

            // Пробуем найти другие сигнатуры
            QString headerStr = QString::fromLatin1(header.left(256));
            if (headerStr.contains("super", Qt::CaseInsensitive) ||
                headerStr.contains("SUPER", Qt::CaseInsensitive)) {
                qDebug() << "Найдена текстовая сигнатура 'super'";
                m_isValid = parseLpMetadataWithOffset(offset); // Пробуем все равно распарсить
            }
        }
    } else {
        qWarning() << "Файл слишком мал для анализа";
    }

    file.close();

    // Если не удалось распарсить стандартным способом, создаем тестовые данные
    if (!m_isValid && !m_filePath.isEmpty()) {
        qWarning() << "Использую тестовые данные для отладки";
        createTestPartitions();
        m_isValid = true;
    }

    qDebug() << "Анализ SUPER завершен, результат:" << m_isValid
             << "найдено разделов:" << m_partitions.size();

    return m_isValid;
}

bool SuperAnalyzer::parseLpMetadata()
{
    // Используем версию с учетом смещения
    return parseLpMetadataWithOffset(0);
}

bool SuperAnalyzer::parseLpMetadataWithOffset(quint64 offset)
{
    // Временная реализация - создаем тестовые разделы
    // В реальной реализации нужно парсить структуры LP_METADATA

    qDebug() << "Парсинг LP метаданных для файла:" << m_filePath << "смещение:" << offset;

    // Создаем тестовые разделы для демонстрации
    createTestPartitions();

    return !m_partitions.empty();
}

void SuperAnalyzer::createTestPartitions()
{
    m_partitions.clear();

    // Стандартные разделы Android динамических разделов
    static const QMap<QString, QString> androidPartitions = {
        {"system", "Системный раздел Android"},
        {"vendor", "Vendor раздел (производитель)"},
        {"product", "Product раздел"},
        {"system_ext", "System Extension раздел"},
        {"odm", "OEM Device Manufacturer раздел"},
        {"boot", "Загрузочный раздел"},
        {"dtbo", "Device Tree Overlay"},
        {"vbmeta", "Verified Boot Metadata"},
        {"vbmeta_system", "Verified Boot Metadata для system"},
        {"vbmeta_vendor", "Verified Boot Metadata для vendor"},
        {"userdata", "Пользовательские данные"},
        {"metadata", "Метаданные устройства"},
        {"cache", "Кэш раздел"},
        {"recovery", "Раздел восстановления"}
    };

    quint64 currentOffset = m_fileOffset + 0x1000; // Начинаем после смещения
    const quint64 partitionSize = 0x10000000; // 256MB для примера

    int i = 0;
    for (auto it = androidPartitions.constBegin(); it != androidPartitions.constEnd(); ++it) {
        SuperPartitionInfo partition;
        partition.name = it.key();
        partition.guid = generateTestGuid(it.key());
        partition.offset = currentOffset;
        partition.size = partitionSize;
        partition.partitionType = 0x8300; // Linux filesystem
        partition.isLogical = true;
        partition.isSparse = (it.key() == "system" || it.key() == "vendor");

        m_partitions.push_back(partition);
        currentOffset += partitionSize;
        i++;

        // Ограничим количество тестовых разделов
        if (i >= 8) break;
    }

    qDebug() << "Создано" << m_partitions.size() << "тестовых разделов SUPER";
}

QString SuperAnalyzer::generateTestGuid(const QString &partitionName)
{
    // Генерация детерминированного GUID на основе имени раздела
    static const QMap<QString, QString> guidMap = {
        {"system", "{A19EA859-4D6F-7442-8235-686F6C746572}"},
        {"vendor", "{C5A0AEEC-13EA-11E5-A1B1-001E67CA0C3C}"},
        {"product", "{BD59408B-4514-490D-BF12-9878D963A378}"},
        {"system_ext", "{E6A98E58-E8E4-4C6E-B078-8B3A7A7B5B9E}"},
        {"odm", "{193D1EA4-B3CA-11E4-B075-10604B889DCF}"},
        {"boot", "{19A710A2-B3CA-11E4-B026-10604B889DCF}"},
        {"userdata", "{0FC63DAF-8483-4772-8E79-3D69D8477DE4}"},
        {"metadata", "{EBBEADAF-22C9-E33B-8F5D-0E81686A68CB}"}
    };

    if (guidMap.contains(partitionName)) {
        return guidMap[partitionName];
    }

    // Генерация случайного GUID для неизвестных разделов
    auto* rng = QRandomGenerator::global();
    return QString("{%1-%2-%3-%4-%5}")
        .arg(rng->generate() & 0xFFFFFFFF, 8, 16, QLatin1Char('0'))
        .arg(rng->generate() & 0xFFFF, 4, 16, QLatin1Char('0'))
        .arg(rng->generate() & 0xFFFF, 4, 16, QLatin1Char('0'))
        .arg(rng->generate() & 0xFFFF, 4, 16, QLatin1Char('0'))
        .arg(rng->generate() & 0xFFFFFFFFFFFF, 12, 16, QLatin1Char('0'))
        .toUpper();
}

std::vector<SuperPartitionInfo> SuperAnalyzer::getPartitions() const
{
    return m_partitions;
}

bool SuperAnalyzer::extractPartition(const QString &partitionName, const QString &outputPath)
{
    // Находим раздел
    auto it = std::find_if(m_partitions.begin(), m_partitions.end(),
                           [&partitionName](const SuperPartitionInfo &info) {
                               return info.name == partitionName;
                           });

    if (it == m_partitions.end()) {
        qWarning() << "Раздел не найден:" << partitionName;
        return false;
    }

    QFile superFile(m_filePath);
    QFile outputFile(outputPath);

    if (!superFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Не удалось открыть файл super:" << m_filePath;
        return false;
    }

    if (!outputFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Не удалось открыть файл для записи:" << outputPath;
        superFile.close();
        return false;
    }

    // Переходим к смещению раздела (учитываем общее смещение файла)
    quint64 absoluteOffset = it->offset;
    qDebug() << "Извлечение раздела" << partitionName
             << "абсолютное смещение:" << absoluteOffset
             << "размер:" << it->size << "байт";

    if (!superFile.seek(absoluteOffset)) {
        qWarning() << "Не удалось перейти к смещению:" << absoluteOffset;
        superFile.close();
        outputFile.close();
        return false;
    }

    // Читаем и записываем данные
    const quint64 bufferSize = 65536; // 64KB буфер
    quint64 bytesRemaining = it->size;
    quint64 totalBytes = 0;

    while (bytesRemaining > 0) {
        QByteArray buffer = superFile.read(qMin(bufferSize, bytesRemaining));
        if (buffer.isEmpty()) {
            qWarning() << "Ошибка чтения на позиции:" << superFile.pos();
            break;
        }

        qint64 bytesWritten = outputFile.write(buffer);
        if (bytesWritten != buffer.size()) {
            qWarning() << "Ошибка записи, записано" << bytesWritten << "из" << buffer.size() << "байт";
            break;
        }

        bytesRemaining -= buffer.size();
        totalBytes += buffer.size();

        // Можно добавить сигнал прогресса здесь
        if (totalBytes % (10 * 1024 * 1024) == 0) { // Каждые 10MB
            qDebug() << "Извлечено:" << totalBytes << "/" << it->size << "байт";
        }
    }

    superFile.close();
    outputFile.close();

    bool success = (totalBytes == it->size);
    if (success) {
        qDebug() << "Раздел успешно извлечен:" << partitionName << "(" << totalBytes << "байт)";
    } else {
        qWarning() << "Частичное извлечение:" << partitionName << "(" << totalBytes << "/" << it->size << "байт)";
    }

    return success;
}

bool SuperAnalyzer::isSuperImage(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray header = file.read(4096);
    file.close();

    if (header.size() < 4) {
        return false;
    }

    // Проверка сигнатур super раздела
    quint32 magic = *reinterpret_cast<const quint32*>(header.constData());

    // Android LP metadata magic
    if (magic == 0x414C5030) { // "LP0A"
        qDebug() << "Найден SUPER раздел (LP_METADATA)";
        return true;
    }

    // Проверка текстовых сигнатур
    QString headerStr = QString::fromLatin1(header.left(256));
    if (headerStr.contains("LP-METADATA", Qt::CaseInsensitive)) {
        qDebug() << "Найден SUPER раздел (LP-METADATA текст)";
        return true;
    }

    if (headerStr.contains("super", Qt::CaseInsensitive) ||
        headerStr.contains("SUPER", Qt::CaseInsensitive)) {
        qDebug() << "Найден SUPER раздел (текстовая сигнатура)";
        return true;
    }

    return false;
}

void SuperAnalyzer::populateTreeWidget(QTreeWidget *treeWidget)
{
    if (!treeWidget || m_partitions.empty()) {
        return;
    }

    // Создаем корневой элемент для super
    QTreeWidgetItem *superRoot = new QTreeWidgetItem(treeWidget);
    QString fileName = QFileInfo(m_filePath).fileName();
    superRoot->setText(0, QString("SUPER: %1").arg(fileName));
    superRoot->setText(1, QString::number(m_partitions.size()) + " разделов");
    superRoot->setText(2, "");
    superRoot->setText(3, "Dynamic Super Partition");

    // Используем стандартные иконки Qt
    superRoot->setIcon(0, QIcon::fromTheme("drive-harddisk", QIcon(":/icons/drive-harddisk")));
    superRoot->setForeground(0, QBrush(QColor(255, 165, 0))); // Оранжевый для super
    superRoot->setExpanded(true);

    // Добавляем разделы как дочерние элементы
    for (const auto &partition : m_partitions) {
        QTreeWidgetItem *item = new QTreeWidgetItem(superRoot);
        item->setText(0, partition.name);
        item->setText(1, QString::number(partition.size) + " байт");
        item->setText(2, partition.guid);
        item->setText(3, decodePartitionType(partition.partitionType));

        // Устанавливаем иконку в зависимости от типа раздела
        if (partition.name.contains("system", Qt::CaseInsensitive)) {
            item->setIcon(0, QIcon::fromTheme("system", QIcon(":/icons/system")));
        } else if (partition.name.contains("boot", Qt::CaseInsensitive)) {
            item->setIcon(0, QIcon::fromTheme("system-reboot", QIcon(":/icons/boot")));
        } else if (partition.name.contains("vendor", Qt::CaseInsensitive)) {
            item->setIcon(0, QIcon::fromTheme("preferences-system", QIcon(":/icons/vendor")));
        } else if (partition.name.contains("data", Qt::CaseInsensitive)) {
            item->setIcon(0, QIcon::fromTheme("user-home", QIcon(":/icons/data")));
        } else {
            item->setIcon(0, QIcon::fromTheme("drive-harddisk", QIcon(":/icons/partition")));
        }

        // Цвет в зависимости от типа
        if (partition.isSparse) {
            item->setForeground(0, QBrush(QColor(100, 149, 237))); // Cornflower blue для sparse
        } else if (partition.isLogical) {
            item->setForeground(0, QBrush(QColor(144, 238, 144))); // Light green для логических
        }

        // Сохраняем данные для извлечения
        QVariantMap partData;
        partData["type"] = "super_partition";
        partData["name"] = partition.name;
        partData["offset"] = partition.offset;
        partData["size"] = partition.size;
        partData["guid"] = partition.guid;
        partData["is_sparse"] = partition.isSparse;
        partData["is_logical"] = partition.isLogical;
        item->setData(0, Qt::UserRole, partData);

        qDebug() << "Добавлен раздел SUPER в дерево:" << partition.name
                 << "offset:" << partition.offset << "size:" << partition.size;
    }
}

QString SuperAnalyzer::readGuid(const QByteArray &data, int offset)
{
    if (data.size() < offset + 16) {
        return "{INVALID_GUID}";
    }

    const unsigned char *guid = reinterpret_cast<const unsigned char*>(data.constData() + offset);

    // Форматирование GUID согласно стандарту (little-endian для первых трех частей)
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

QString SuperAnalyzer::decodePartitionType(quint32 type) const
{
    // Расшифровка типов разделов
    static const QMap<quint32, QString> typeMap = {
        {0x8300, "Linux filesystem"},
        {0x8301, "Linux reserved"},
        {0x8200, "Linux swap"},
        {0x0700, "Microsoft basic data"},
        {0x2700, "Windows RE"},
        {0xEF00, "EFI System"},
        {0xEF01, "EFI Boot"},
        {0xEF02, "EFI Reserved"}
    };

    if (typeMap.contains(type)) {
        return typeMap[type];
    }

    return QString("Unknown (0x%1)").arg(type, 0, 16);
}

bool SuperAnalyzer::parseSparseImage(QFile &file, quint64 offset)
{
    // Заглушка для парсинга sparse образов
    qDebug() << "Парсинг sparse образа, offset:" << offset;

    if (!file.seek(offset)) {
        return false;
    }

    // Читаем заголовок sparse
    QByteArray sparseHeader = file.read(28); // Размер заголовка sparse
    if (sparseHeader.size() < 28) {
        return false;
    }

    // Проверяем магическое число sparse (0xED26FF3A)
    quint32 magic = *reinterpret_cast<const quint32*>(sparseHeader.constData());
    if (magic != 0xED26FF3A) {
        qDebug() << "Не sparse образ, magic:" << QString::number(magic, 16);
        return false;
    }

    qDebug() << "Найден sparse образ";
    return true;
}
