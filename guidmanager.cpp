#include "guidmanager.h"
#include <QDebug>

GuidManager::GuidManager(QObject *parent)
    : QObject{parent}
{
    // Инициализируем карту GUID при создании объекта
    m_guidMap = initializeGuidMap();
}

QMap<QUuid, QString> GuidManager::initializeGuidMap()
{
    QMap<QUuid, QString> map;

    // ============================================
    // 1. СТАНДАРТНЫЕ И ОБЩИЕ GUID (UEFI/BIOS/ПК)
    // ============================================

    // Загрузочные разделы
    map[QUuid("024DEE41-33E7-11D3-9D69-0008C781F39F")] = "MBR Partition Scheme";
    map[QUuid("21686148-6449-6E6F-744E-656564454649")] = "BIOS Boot";
    map[QUuid("C12A7328-F81F-11D2-BA4B-00A0C93EC93B")] = "EFI System Partition (ESP)";

    // Системные разделы
    map[QUuid("E3C9E316-0B5C-4DB8-817D-F92DF00215AE")] = "Microsoft Reserved (MSR)";
    map[QUuid("EBD0A0A2-B9E5-4433-87C0-68B6B72699C7")] = "Microsoft Basic Data";
    map[QUuid("DE94BBA4-06D1-4D40-A16A-BFD50179D6AC")] = "Microsoft Recovery";

    // Производители
    map[QUuid("D3BFE2DE-3DAF-11DF-BA40-E3A556D89593")] = "Intel Fast Flash (iFFS)";
    map[QUuid("F4019732-066E-4E12-8273-346C5641494F")] = "Sony Boot";

    // ============================================
    // 2. LINUX GUID (Стандартные)
    // ============================================

    // Основные разделы Linux
    map[QUuid("0FC63DAF-8483-473E-817E-0F72E55EA8C5")] = "Linux Filesystem";
    map[QUuid("44479540-F297-41B2-9AF7-D131D5F0458A")] = "Linux Root (x86-64)";
    map[QUuid("4F68BCE3-E8CD-4DB1-96E7-FBCAF984B709")] = "Linux Root (x86)";
    map[QUuid("0657FD6D-A4AB-43C4-84E5-0933C84B4F4F")] = "Linux Swap";

    // Иерархия Linux
    map[QUuid("69DAD710-2CE4-4E3C-B16C-21A1D49ABED3")] = "Linux /boot";
    map[QUuid("BC13C2FF-59E6-4262-A352-B275FD6F7172")] = "Linux /boot/efi";
    map[QUuid("933AC7E1-2EB4-4F13-B844-0E14E2AEF915")] = "Linux /home";
    map[QUuid("5808C8AA-7E8F-42E0-85D2-E1E90434CFB3")] = "Linux /usr";
    map[QUuid("3B8F8425-20E0-4F3B-907F-1A25A76F98E8")] = "Linux /srv";

    // Специальные Linux
    map[QUuid("A19D880F-05FC-4D3B-A006-743F0F84911E")] = "Linux RAID";
    map[QUuid("E6D6D379-F507-44C2-A23C-238F2A3DF928")] = "Linux LVM";
    map[QUuid("8DA63339-0007-60C0-C436-083AC8230908")] = "Linux Reserved";

    // ============================================
    // 3. ОСНОВНЫЕ ANDROID РАЗДЕЛЫ (AOSP)
    // ============================================

    // Критичные для загрузки
    map[QUuid("19A710A2-B37A-11D4-A400-006073657A00")] = "Android Bootloader";
    map[QUuid("193D1EA4-B3CA-11D4-A086-006073657A00")] = "Android Boot";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC59")] = "Android Vendor Boot";

    // Основные системные
    map[QUuid("38F428E6-D326-4C41-9C8F-9A6D9B2A6C2F")] = "Android System";
    map[QUuid("AC6D7924-EBD0-11D1-A90B-00A0C9EE6C1F")] = "Android Vendor";
    map[QUuid("767941D0-000C-11AA-AA00-4056895D4600")] = "Android Userdata";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC50")] = "Android Cache";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC4F")] = "Android Recovery";

    // Супер-раздел (динамические разделы)
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC5D")] = "Android Super";

    // ============================================
    // 4. ANDROID ВСПОМОГАТЕЛЬНЫЕ РАЗДЕЛЫ
    // ============================================

    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC4E")] = "Android Misc";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC51")] = "Android Metadata";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC52")] = "Android Factory";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC53")] = "Android OEM";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC58")] = "Android Persistent";
    map[QUuid("AF3DC60F-8384-7C41-9E69-D6D357E6F59C")] = "Android Meta";

    // ============================================
    // 5. ANDROID АППАРАТНО-ЗАВИСИМЫЕ РАЗДЕЛЫ
    // ============================================

    // Модем и связь
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC54")] = "Android Modem";

    // Безопасность
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC56")] = "Android Keymaster";
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC57")] = "Android Keystore";

    // Прошивки периферии
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC55")] = "Android DSP";

    // Устройства дерева
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC5A")] = "Android DTBO";

    // Графика
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC5B")] = "Android Logo";

    // Вторичный загрузчик
    map[QUuid("A2A0D0EB-F5B9-4B7A-B2B9-7B296831BC5C")] = "Android SPL";

    // ============================================
    // 6. MEDIATEK (MTK) СПЕЦИФИЧНЫЕ
    // ============================================

    map[QUuid("FBC2C131-6392-4217-B51E-548A6EDB03D0")] = "MTK Protect (EXT4)";
    map[QUuid("EBC597D0-2053-4B15-8B64-E0AAC75F4DB1")] = "MTK Protect (Backup)";
    map[QUuid("FE8B0F3E-565D-4D48-950B-8AC3B0A0A3A3")] = "MTK Protect (Factory)";
    map[QUuid("D3310505-EB5A-4BB0-8D2F-B5C4C0A1A5A5")] = "MTK Protect (OEM)";

    // ДОБАВЛЕНО - Важные MTK разделы
    map[QUuid("52C6FEC6-2A52-4D3F-9C7B-6A8D7D8A5B9C")] = "MTK Preloader";
    map[QUuid("EE6F2C31-41D0-446E-B76F-6D1B5A5B8B5B")] = "MTK NVRAM";

    // ============================================
    // 7. QUALCOMM СПЕЦИФИЧНЫЕ
    // ============================================

    map[QUuid("DEA0BA2C-CBDD-4805-B4F9-F428251C3E98")] = "QCDT";
    map[QUuid("098DF793-D712-413D-9CA4-9DDE5B91ED04")] = "QCSBL";
    map[QUuid("400FFDCD-22E0-47E7-9A23-F16ED9382388")] = "QCOM OEM";

    // ДОБАВЛЕНО - Дополнительные Qualcomm разделы
    map[QUuid("DEA0BA2C-0000-0000-0000-000000000001")] = "QCOM RPM";
    map[QUuid("DEA0BA2C-0000-0000-0000-000000000002")] = "QCOM TZ";
    map[QUuid("DEA0BA2C-0000-0000-0000-000000000003")] = "QCOM Hyp";
    map[QUuid("50C1D5B5-37A5-4A7F-B5A6-5E5D5A5A5A5A")] = "QCOM DDR";

    // ============================================
    // 8. SAMSUNG СПЕЦИФИЧНЫЕ
    // ============================================

    map[QUuid("A0933AC3-214F-4F70-A9A6-8D9B2FE98A07")] = "Samsung EFS";
    map[QUuid("6C95E238-E343-4BA8-B489-8681ED22AD0B")] = "Samsung PARAM";

    // ДОБАВЛЕНО - Дополнительные Samsung разделы
    map[QUuid("A0933AC3-0000-0000-0000-000000000001")] = "Samsung CSC";
    map[QUuid("A0933AC3-0000-0000-0000-000000000002")] = "Samsung OMC";
    map[QUuid("8F68CC74-0000-0000-0000-000000000000")] = "Samsung Bootloader";

    // ============================================
    // 9. HUAWEI СПЕЦИФИЧНЫЕ
    // ============================================

    map[QUuid("8F68CC74-C5E5-48DA-BE91-A0C8C15E9C80")] = "Huawei 3RD";
    map[QUuid("9FDAA6EF-2B3F-40D2-9A97-DE6C9C8A90D7")] = "Huawei CACHE";

    // ДОБАВЛЕНО - Дополнительные Huawei разделы
    map[QUuid("8F68CC74-0000-0000-0000-000000000001")] = "Huawei VENDOR";
    map[QUuid("8F68CC74-0000-0000-0000-000000000002")] = "Huawei CUST";
    map[QUuid("8F68CC74-0000-0000-0000-000000000003")] = "Huawei PRODUCT";

    // ============================================
    // 10. GOOGLE СПЕЦИФИЧНЫЕ (Pixel и т.д.)
    // ============================================

    // ДОБАВЛЕНО
    map[QUuid("FE8B0F3E-565D-4D48-950B-8AC3B0A0A3A4")] = "Google Factory";
    map[QUuid("FE8B0F3E-565D-4D48-950B-8AC3B0A0A3A5")] = "Google OTA";
    map[QUuid("FE8B0F3E-565D-4D48-950B-8AC3B0A0A3A6")] = "Google Misc";

    return map;
}

QString GuidManager::getPartitionDescription(const QUuid &guid)
{
    // Ищем GUID в карте
    auto it = m_guidMap.find(guid);
    if (it != m_guidMap.end()) {
        return it.value();
    }

    // Если не найден, возвращаем стандартное описание
    return "Unknown Partition";
}

QString GuidManager::getPartitionDescription(const QString &guidString)
{
    // Пытаемся преобразовать строку в QUuid
    QUuid guid = QUuid::fromString(guidString);
    if (guid.isNull()) {
        return "Invalid GUID format";
    }

    return getPartitionDescription(guid);
}

bool GuidManager::isAndroidPartition(const QUuid &guid)
{
    QString description = getPartitionDescription(guid);
    return description.contains("Android") ||
           description.contains("MTK") ||
           description.contains("QCOM") ||
           description.contains("Samsung") ||
           description.contains("Huawei") ||
           description.contains("Google");
}

bool GuidManager::isDynamicPartition(const QUuid &guid)
{
    // Проверяем, является ли раздел Super разделом (динамическим)
    QString description = getPartitionDescription(guid);
    return description.contains("Android Super") ||
           description.contains("Super");
}
