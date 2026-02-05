#include "guidmanager.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QDebug>
#include <QMap>
#include <QString>
#include <QByteArray>

GuidManager::GuidManager(QObject *parent)
    : QObject(parent)
{
    initGuidDatabase();
}

void GuidManager::initGuidDatabase()
{
    // Расширенная база данных GUID для Android и стандартных разделов
    m_guidMap = {
        // Стандартные GUID разделов
        {"C12A7328-F81F-11D2-BA4B-00A0C93EC93B", {"EFI System", "Системный раздел EFI"}},
        {"024DEE41-33E7-11D3-9D69-0008C781F39F", {"MBR", "Главная загрузочная запись"}},
        {"E3C9E316-0B5C-4DB8-817D-F92DF00215AE", {"Microsoft Reserved", "Зарезервировано Microsoft"}},
        {"EBD0A0A2-B9E5-4433-87C0-68B6B72699C7", {"Microsoft Basic Data", "Основные данные Windows"}},

        // Android специфичные GUID
        {"19A710A2-B3CA-11E4-B026-10604B889DCF", {"Android Bootloader", "Загрузчик Android"}},
        {"193D1EA4-B3CA-11E4-B075-10604B889DCF", {"Android Boot", "Раздел boot Android"}},
        {"0DE68BC2-3C0F-4D71-BF27-A2C13461FCD0", {"Android Recovery", "Раздел recovery"}},
        {"A19EA859-4D6F-7442-8235-686F6C746572", {"Android System", "Системный раздел Android"}},
        {"C5A0AEEC-13EA-11E5-A1B1-001E67CA0C3C", {"Android Vendor", "Vendor раздел"}},
        {"BD59408B-4514-490D-BF12-9878D963A378", {"Android Userdata", "Пользовательские данные"}},
        {"EBBEADAF-22C9-E33B-8F5D-0E81686A68CB", {"Android Cache", "Кэш раздел"}},
        {"DC76DDA9-5AC1-491C-AF42-A82591580C0D", {"Android Metadata", "Метаданные Android"}},

        // Динамические разделы (Super)
        {"E6A98E58-E8E4-4C6E-B078-8B3A7A7B5B9E", {"Android Dynamic", "Динамический раздел Android"}},
        {"2C86E742-745E-4FDD-BFD1-B930C9A6F4C1", {"Android Logical", "Логический раздел Android"}},

        // Файловые системы
        {"0FC63DAF-8483-4772-8E79-3D69D8477DE4", {"Linux Filesystem", "Файловая система Linux"}},
        {"0657FD6D-A4AB-43C4-84E5-0933C84B4F4F", {"Linux Swap", "Раздел подкачки Linux"}},
        {"E6D6D379-F507-44C2-A23C-238F2A3DF928", {"Linux LVM", "Менеджер логических томов"}},

        // GPT служебные
        {"8DA63339-0007-60C0-C436-083AC8230908", {"GPT Reserved", "Зарезервировано GPT"}},
        {"00000000-0000-0000-0000-000000000000", {"Empty", "Пустой раздел"}}
    };

    qDebug() << "База данных GUID инициализирована, записей:" << m_guidMap.size();
}

QString GuidManager::getGuidDescription(const QString &guid) const
{
    QString normalized = guid.toUpper().trimmed();

    // Убираем фигурные скобки если есть
    if (normalized.startsWith('{') && normalized.endsWith('}')) {
        normalized = normalized.mid(1, normalized.length() - 2);
    }

    // Прямой поиск
    auto it = m_guidMap.find(normalized);
    if (it != m_guidMap.end()) {
        return it.value().second; // Возвращаем описание
    }

    // Поиск по частичному совпадению (для Android)
    for (auto mapIt = m_guidMap.constBegin(); mapIt != m_guidMap.constEnd(); ++mapIt) {
        if (normalized.contains(mapIt.key())) {
            return mapIt.value().second;
        }
    }

    return "Неизвестный GUID";
}

void GuidManager::displayGuidDetails(const QString &guid, QTableWidget *tableWidget)
{
    if (!tableWidget) {
        return;
    }

    // Очищаем таблицу
    tableWidget->setRowCount(0);

    // Настройка таблицы
    tableWidget->setAlternatingRowColors(true);
    tableWidget->verticalHeader()->setVisible(false);
    tableWidget->horizontalHeader()->setStretchLastSection(true);

    // Добавляем информацию о GUID
    int row = tableWidget->rowCount();
    tableWidget->insertRow(row);

    tableWidget->setItem(row, 0, new QTableWidgetItem(guid));

    // Поиск описания в базе данных
    QString normalized = guid.toUpper().trimmed();
    if (normalized.startsWith('{') && normalized.endsWith('}')) {
        normalized = normalized.mid(1, normalized.length() - 2);
    }

    auto it = m_guidMap.find(normalized);
    if (it != m_guidMap.end()) {
        tableWidget->setItem(row, 1, new QTableWidgetItem(it.value().first));
        tableWidget->setItem(row, 2, new QTableWidgetItem(it.value().second));
    } else {
        // Проверяем частичное совпадение
        bool found = false;
        for (auto mapIt = m_guidMap.constBegin(); mapIt != m_guidMap.constEnd(); ++mapIt) {
            if (normalized.contains(mapIt.key())) {
                tableWidget->setItem(row, 1, new QTableWidgetItem(mapIt.value().first));
                tableWidget->setItem(row, 2, new QTableWidgetItem(mapIt.value().second));
                found = true;
                break;
            }
        }

        if (!found) {
            tableWidget->setItem(row, 1, new QTableWidgetItem("Unknown"));
            tableWidget->setItem(row, 2, new QTableWidgetItem("Неизвестный GUID"));
        }
    }

    // Добавляем дополнительную информацию
    addGuidMetadata(guid, tableWidget);
}

void GuidManager::addGuid(const QString &guid, const QString &type, const QString &description)
{
    QString normalized = guid.toUpper().trimmed();
    if (normalized.startsWith('{') && normalized.endsWith('}')) {
        normalized = normalized.mid(1, normalized.length() - 2);
    }

    m_guidMap[normalized] = qMakePair(type, description);
}

bool GuidManager::hasGuid(const QString &guid) const
{
    QString normalized = guid.toUpper().trimmed();
    if (normalized.startsWith('{') && normalized.endsWith('}')) {
        normalized = normalized.mid(1, normalized.length() - 2);
    }

    return m_guidMap.contains(normalized);
}

void GuidManager::addGuidMetadata(const QString &guid, QTableWidget *tableWidget)
{
    // Анализ структуры GUID
    QString normalized = guid.toUpper().trimmed();

    // Убираем фигурные скобки если есть
    if (normalized.startsWith('{') && normalized.endsWith('}')) {
        normalized = normalized.mid(1, normalized.length() - 2);
    }

    // Проверяем формат GUID
    if (normalized.length() == 36 && normalized.count('-') == 4) {
        // Извлекаем компоненты GUID
        QStringList parts = normalized.split('-');

        // Определяем версию GUID по первому компоненту
        QString timeLow = parts[0];
        quint32 versionField = parts[2].left(1).toUInt(nullptr, 16);

        QString version;
        switch (versionField) {
        case 1: version = "Version 1 (Time-based)"; break;
        case 2: version = "Version 2 (DCE Security)"; break;
        case 3: version = "Version 3 (MD5 Hash)"; break;
        case 4: version = "Version 4 (Random)"; break;
        case 5: version = "Version 5 (SHA-1 Hash)"; break;
        default: version = "Unknown Version";
        }

        int row = tableWidget->rowCount();
        tableWidget->insertRow(row);
        tableWidget->setItem(row, 0, new QTableWidgetItem("Версия GUID"));
        tableWidget->setItem(row, 1, new QTableWidgetItem(version));
        tableWidget->setItem(row, 2, new QTableWidgetItem(""));
    }
}

QString GuidManager::formatGuid(const unsigned char *bytes) const
{
    if (!bytes) {
        return "{INVALID_GUID}";
    }

    return QString("{%1%2%3%4-%5%6-%7%8-%9%10-%11%12%13%14%15%16}")
        .arg(bytes[3], 2, 16, QLatin1Char('0'))
        .arg(bytes[2], 2, 16, QLatin1Char('0'))
        .arg(bytes[1], 2, 16, QLatin1Char('0'))
        .arg(bytes[0], 2, 16, QLatin1Char('0'))
        .arg(bytes[5], 2, 16, QLatin1Char('0'))
        .arg(bytes[4], 2, 16, QLatin1Char('0'))
        .arg(bytes[7], 2, 16, QLatin1Char('0'))
        .arg(bytes[6], 2, 16, QLatin1Char('0'))
        .arg(bytes[8], 2, 16, QLatin1Char('0'))
        .arg(bytes[9], 2, 16, QLatin1Char('0'))
        .arg(bytes[10], 2, 16, QLatin1Char('0'))
        .arg(bytes[11], 2, 16, QLatin1Char('0'))
        .arg(bytes[12], 2, 16, QLatin1Char('0'))
        .arg(bytes[13], 2, 16, QLatin1Char('0'))
        .arg(bytes[14], 2, 16, QLatin1Char('0'))
        .arg(bytes[15], 2, 16, QLatin1Char('0'))
        .toUpper();
}


//05022026-1535

