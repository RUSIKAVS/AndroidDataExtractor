#include "DeviceInfoParser.hpp"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QCryptographicHash>
#include <QtEndian>

DeviceInfo DeviceInfoParser::parseFromDirectory(const QString& path)
{
    DeviceInfo info;

    QDir dir(path);
    if (!dir.exists()) {
        return info;
    }

    // Сначала проверяем наличие EWC файла
    QString ewcFile = dir.absoluteFilePath("device.ewc");
    if (QFile::exists(ewcFile)) {
        DeviceInfo ewcInfo = parseEwcFile(ewcFile);

        // Объединяем информацию из EWC
        if (!ewcInfo.internalModel.isEmpty() && ewcInfo.internalModel != "Unknown") {
            info.internalModel = ewcInfo.internalModel;
        }
        if (!ewcInfo.extractionMethod.isEmpty() && ewcInfo.extractionMethod != "Unknown") {
            info.extractionMethod = ewcInfo.extractionMethod;
        }
        if (ewcInfo.extractionTime.isValid()) {
            info.extractionTime = ewcInfo.extractionTime;
        }
        if (!ewcInfo.brand.isEmpty() && ewcInfo.brand != "Unknown") {
            info.brand = ewcInfo.brand;
        }
        if (!ewcInfo.model.isEmpty() && ewcInfo.model != "Unknown") {
            info.model = ewcInfo.model;
        }
        if (!ewcInfo.androidVersion.isEmpty() && ewcInfo.androidVersion != "Unknown") {
            info.androidVersion = ewcInfo.androidVersion;
        }
    }

    // Определяем платформу
    info.platform = detectPlatform(path);

    // Ищем build.prop файлы
    QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);

    for (const QString& file : files) {
        QString fullPath = dir.absoluteFilePath(file);

        if (file.contains("build.prop", Qt::CaseInsensitive) ||
            file.contains("default.prop", Qt::CaseInsensitive)) {
            DeviceInfo propInfo = parseBuildProp(fullPath);

            // Объединяем информацию (только если не заполнено из EWC)
            if (info.brand.isEmpty() || info.brand == "Unknown") {
                info.brand = propInfo.brand;
            }
            if (info.model.isEmpty() || info.model == "Unknown") {
                info.model = propInfo.model;
            }
            if (info.androidVersion.isEmpty() || info.androidVersion == "Unknown") {
                info.androidVersion = propInfo.androidVersion;
            }
            if (info.firmwareDate.isEmpty() || info.firmwareDate == "Unknown") {
                info.firmwareDate = propInfo.firmwareDate;
            }
            if (info.securityPatchDate.isEmpty() || info.securityPatchDate == "Unknown") {
                info.securityPatchDate = propInfo.securityPatchDate;
            }
            if (info.serial.isEmpty() || info.serial == "Unknown") {
                info.serial = propInfo.serial;
            }
            if (info.imei1.isEmpty() || info.imei1 == "Unknown") {
                info.imei1 = propInfo.imei1;
            }
            if (info.imei2.isEmpty() || info.imei2 == "Unknown") {
                info.imei2 = propInfo.imei2;
            }
        }
        else if (file.endsWith(".bin", Qt::CaseInsensitive) ||
                 file.endsWith(".img", Qt::CaseInsensitive)) {
            // Анализируем образы на наличие информации
            DeviceInfo imageInfo = analyzeImageForDeviceInfo(fullPath);

            if (info.imei1.isEmpty() || info.imei1 == "Unknown") {
                info.imei1 = imageInfo.imei1;
            }
            if (info.imei2.isEmpty() || info.imei2 == "Unknown") {
                info.imei2 = imageInfo.imei2;
            }
            if (info.serial.isEmpty() || info.serial == "Unknown") {
                info.serial = imageInfo.serial;
            }
            if (info.platform == "Unknown") {
                info.platform = imageInfo.platform;
            }
        }
    }

    return info;
}

QString DeviceInfoParser::detectPlatform(const QString& path)
{
    QDir dir(path);
    QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);

    for (const QString& file : files) {
        QString fullPath = dir.absoluteFilePath(file);
        QFile f(fullPath);

        if (f.open(QIODevice::ReadOnly)) {
            QByteArray data = f.read(4096);
            f.close();

            QString platform = detectPlatformFromHeader(data);
            if (platform != "Unknown") {
                return platform;
            }
        }

        // Проверяем по имени файла
        if (file.contains("mtk", Qt::CaseInsensitive)) {
            return "MTK";
        } else if (file.contains("sprd", Qt::CaseInsensitive)) {
            return "SPD";
        } else if (file.contains("exynos", Qt::CaseInsensitive)) {
            return "EXN";
        } else if (file.contains("qc", Qt::CaseInsensitive) ||
                   file.contains("qualcomm", Qt::CaseInsensitive)) {
            return "QCM";
        } else if (file.contains("kirin", Qt::CaseInsensitive)) {
            return "KRN";
        }
    }

    return "Unknown";
}

QString DeviceInfoParser::detectPlatformFromHeader(const QByteArray& header)
{
    if (header.size() < 512) {
        return "Unknown";
    }

    // Создаем QByteArray с правильными размерами для паттернов
    QByteArray mtkPattern1 = QByteArray::fromHex("4d544b00"); // "MTK\0"
    QByteArray mtkPattern2 = QByteArray::fromHex("88888888");
    QByteArray mtkPattern3 = QByteArray::fromHex("cececece");

    // MTK - MediaTek
    if (header.contains("MTK") ||
        header.contains("Mediatek") ||
        header.contains("MediaTek") ||
        header.contains(mtkPattern1) ||
        header.contains(mtkPattern2) ||
        header.contains(mtkPattern3) ||
        header.contains(QByteArray::fromHex("4d544b")) ||  // "MTK"
        (header.size() > 256 && (
             header.mid(0, 3) == QByteArray::fromHex("4d544b") ||  // MTK
             header.contains(QByteArray::fromHex("4d544d4d")) ||    // MTMM
             header.contains(QByteArray::fromHex("4b544d"))        // KTM
             ))) {
        return "MTK";
    }

    // Spreadtrum (SPD)
    if (header.contains("SPRD") ||
        header.contains("spreadtrum") ||
        header.contains("Spreadtrum") ||
        (header.size() > 100 && (
             header.mid(0, 4) == "SPRD" ||
             header.contains(QByteArray::fromHex("53505244")) // "SPRD"
             ))) {
        return "SPD";
    }

    // Exynos (Samsung)
    if (header.contains("EXYNOS") ||
        header.contains("exynos") ||
        header.contains("Exynos") ||
        (header.size() > 100 && (
             header.mid(0, 6) == "EXYNOS" ||
             header.contains(QByteArray::fromHex("4558594e4f53")) // "EXYNOS"
             ))) {
        return "EXN";
    }

    // Qualcomm
    if (header.contains("QC_IMAGE") ||
        header.contains("QUALCOMM") ||
        header.contains("qualcomm") ||
        header.contains("MSM") ||
        header.contains("SDM") ||
        header.contains("qcom") ||
        header.contains("QCOM") ||
        (header.size() > 256 && (
             header.contains(QByteArray::fromHex("51435f494d414745")) || // "QC_IMAGE"
             header.contains(QByteArray::fromHex("5155414c434f4d4d")) || // "QUALCOMM"
             header.contains(QByteArray::fromHex("4d534d")) ||           // "MSM"
             header.contains(QByteArray::fromHex("53444d"))              // "SDM"
             ))) {
        return "QCM";
    }

    // Kirin (Huawei)
    if (header.contains("KIRIN") ||
        header.contains("kirin") ||
        header.contains("Kirin") ||
        header.contains("HI36") ||
        header.contains("HI62") ||
        header.contains("hi36") ||
        header.contains("hi62") ||
        (header.size() > 256 && (
             header.contains(QByteArray::fromHex("4b4952494e")) || // "KIRIN"
             header.contains(QByteArray::fromHex("48493336")) ||   // "HI36"
             header.contains(QByteArray::fromHex("48493632"))      // "HI62"
             ))) {
        return "KRN";
    }

    // Unisoc (он же Spreadtrum, но может быть отдельно)
    if (header.contains("UNISOC") ||
        header.contains("unisoc") ||
        header.contains("Unisoc")) {
        return "SPD"; // Или можно вернуть "UNI"
    }

    // Samsung (может использовать Exynos или Qualcomm)
    if (header.contains("SAMSUNG") ||
        header.contains("samsung") ||
        header.contains("SEC_")) {
        // Требуется дополнительный анализ для определения чипсета
        return "Samsung";
    }

    // Google (Pixel)
    if (header.contains("GOOGLE") ||
        header.contains("google") ||
        header.contains("Pixel") ||
        header.contains("pixel")) {
        // Pixel обычно использует Qualcomm
        return "QCM";
    }

    // Xiaomi
    if (header.contains("XIAOMI") ||
        header.contains("xiaomi") ||
        header.contains("Redmi") ||
        header.contains("redmi")) {
        // Xiaomi использует разные платформы, нужен дополнительный анализ
        return "Unknown";
    }

    // Проверяем по эвристике - ищем известные паттерны в фиксированных смещениях
    if (header.size() >= 1024) {
        // MTK часто имеет специфичные структуры в определенных смещениях
        for (int offset = 0; offset < 1024; offset += 4) {
            if (offset + 4 <= header.size()) {
                quint32 value = qFromLittleEndian<quint32>(
                    reinterpret_cast<const uchar*>(header.constData() + offset));

                // MTK магические числа
                if (value == 0x4D4D4D4D ||  // "MMMM"
                    value == 0x4B544D00 ||  // "KTM\0"
                    value == 0x88888888 ||  // MTK паттерн
                    value == 0xCECECECE) {  // Еще MTK паттерн
                    return "MTK";
                }

                // Qualcomm магические числа
                if (value == 0x474d4951 ||  // "QMIG" (обратный порядок)
                    value == 0x4f4c4351) {  // "QCLO" (обратный порядок)
                    return "QCM";
                }
            }
        }
    }

    return "Unknown";
}

DeviceInfo DeviceInfoParser::parseBuildProp(const QString& filePath)
{
    DeviceInfo info;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return info;
    }

    QTextStream stream(&file);

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        if (line.startsWith("ro.product.brand=")) {
            info.brand = line.mid(17).replace("\"", "").trimmed();
        } else if (line.startsWith("ro.product.model=")) {
            info.model = line.mid(17).replace("\"", "").trimmed();
        } else if (line.startsWith("ro.build.version.release=")) {
            info.androidVersion = line.mid(25).replace("\"", "").trimmed();
        } else if (line.startsWith("ro.build.display.id=")) {
            QString displayId = line.mid(20).replace("\"", "").trimmed();
            // Извлекаем дату из display id
            QRegularExpression dateRegex("\\d{4}[-.]\\d{2}[-.]\\d{2}");
            QRegularExpressionMatch match = dateRegex.match(displayId);
            if (match.hasMatch()) {
                info.firmwareDate = match.captured().replace(".", "-");
            }
        } else if (line.startsWith("ro.build.version.security_patch=")) {
            QString patch = line.mid(33).replace("\"", "").trimmed();
            // Форматируем дату патча безопасности
            if (patch.length() >= 7) { // ГГГГ-ММ
                info.securityPatchDate = patch;
            }
        } else if (line.startsWith("ro.serialno=")) {
            info.serial = line.mid(12).replace("\"", "").trimmed();
        } else if (line.startsWith("persist.radio.imei=") ||
                   line.startsWith("ro.ril.oem.imei=") ||
                   line.startsWith("gsm.imei=")) {
            QString imei = line.split("=")[1].replace("\"", "").trimmed();
            // Разделяем несколько IMEI если есть
            if (imei.contains(",")) {
                QStringList imeiList = imei.split(",");
                if (!imeiList.isEmpty() && isLikelyImei(imeiList[0].trimmed())) {
                    info.imei1 = imeiList[0].trimmed();
                }
                if (imeiList.size() > 1 && isLikelyImei(imeiList[1].trimmed())) {
                    info.imei2 = imeiList[1].trimmed();
                }
            } else if (info.imei1.isEmpty() && isLikelyImei(imei)) {
                info.imei1 = imei;
            }
        } else if (line.startsWith("persist.radio.imei1=")) {
            QString imei = line.split("=")[1].replace("\"", "").trimmed();
            if (info.imei1.isEmpty() && isLikelyImei(imei)) {
                info.imei1 = imei;
            }
        } else if (line.startsWith("persist.radio.imei2=")) {
            QString imei = line.split("=")[1].replace("\"", "").trimmed();
            if (info.imei2.isEmpty() && isLikelyImei(imei)) {
                info.imei2 = imei;
            }
        }
    }

    file.close();
    return info;
}

QString DeviceInfoParser::extractFromBuildProp(const QString& filePath, const QString& key)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QTextStream stream(&file);
    QString result;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.startsWith(key + "=")) {
            result = line.mid(key.length() + 1).replace("\"", "").trimmed();
            break;
        }
    }

    file.close();
    return result;
}

DeviceInfo DeviceInfoParser::analyzeImageForDeviceInfo(const QString& filePath)
{
    DeviceInfo info;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return info;
    }

    // Читаем первые 128KB для анализа
    QByteArray data = file.read(131072);
    file.close();

    if (data.isEmpty()) {
        return info;
    }

    // Ищем IMEI (15-16 цифр)
    QString dataStr = QString::fromLatin1(data);

    // Ищем IMEI в различных форматах
    QRegularExpression imeiRegex1("\\b\\d{15}\\b");  // Стандартный IMEI (15 цифр)
    QRegularExpression imeiRegex2("\\b\\d{16}\\b");  // IMEISV (16 цифр)
    QRegularExpression imeiRegex3("IMEI[\\s:=]+(\\d{15,16})", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression imeiRegex4("\\b35\\d{13}\\b");  // IMEI начинается с 35...

    QVector<QString> foundImeis;

    // Ищем по всем шаблонам
    auto searchImeis = [&](const QRegularExpression& regex) {
        QRegularExpressionMatchIterator it = regex.globalMatch(dataStr);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString imei = match.captured();
            if (imei.length() == 1) { // Если есть группы захвата
                imei = match.captured(1);
            }
            if (isLikelyImei(imei) && !foundImeis.contains(imei)) {
                foundImeis.append(imei);
            }
        }
    };

    searchImeis(imeiRegex1);
    searchImeis(imeiRegex2);
    searchImeis(imeiRegex3);
    searchImeis(imeiRegex4);

    // Упорядочиваем найденные IMEI
    if (!foundImeis.isEmpty()) {
        info.imei1 = foundImeis[0];
        if (foundImeis.size() > 1) {
            info.imei2 = foundImeis[1];
        }
    }

    // Ищем серийный номер
    QRegularExpression serialRegex("\\b[A-Z0-9]{8,20}\\b");
    QRegularExpressionMatchIterator serialIt = serialRegex.globalMatch(dataStr);

    while (serialIt.hasNext()) {
        QRegularExpressionMatch match = serialIt.next();
        QString serial = match.captured();

        // Проверяем, что это не IMEI и не просто число
        if (!isLikelyImei(serial) &&
            serial.length() >= 8 &&
            serial != info.imei1 &&
            serial != info.imei2) {

            // Проверяем, содержит ли буквы (серийные номера обычно содержат буквы)
            if (serial.contains(QRegularExpression("[A-Z]"))) {
                info.serial = serial;
                break;
            }
        }
    }

    // Ищем маркеры платформы в заголовке
    info.platform = detectPlatformFromHeader(data.left(4096));

    return info;
}

bool DeviceInfoParser::isLikelyImei(const QString& imei)
{
    if (imei.length() < 15 || imei.length() > 16) {
        return false;
    }

    // Проверяем что все символы - цифры
    for (QChar ch : imei) {
        if (!ch.isDigit()) {
            return false;
        }
    }

    // Проверяем по алгоритму Луна (Luhn) для IMEI
    int sum = 0;
    bool alternate = false;

    for (int i = imei.length() - 1; i >= 0; --i) {
        int n = imei[i].digitValue();

        if (alternate) {
            n *= 2;
            if (n > 9) {
                n = (n % 10) + 1;
            }
        }

        sum += n;
        alternate = !alternate;
    }

    return (sum % 10 == 0);
}

QString DeviceInfoParser::readEwcFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return "Cannot read device.ewc file";
    }

    QTextStream stream(&file);
    QString info;
    bool firstSection = true;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        if (line.isEmpty() || line.startsWith(";") || line.startsWith("#")) {
            continue;
        }

        if (line.startsWith("[") && line.endsWith("]")) {
            if (!firstSection) {
                info += "\n";
            }
            info += line + "\n";
            firstSection = false;
        } else if (line.contains("=")) {
            QStringList parts = line.split("=");
            if (parts.size() == 2) {
                QString key = parts[0].trimmed();
                QString value = parts[1].trimmed();

                // Форматируем с выравниванием
                info += QString("  %1: %2\n").arg(key, -25).arg(value);
            }
        }
    }

    file.close();
    return info;
}

DeviceInfo DeviceInfoParser::parseEwcFile(const QString& filePath)
{
    DeviceInfo info;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return info;
    }

    QTextStream stream(&file);

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        if (line.isEmpty() || line.startsWith(";") || line.startsWith("#")) {
            continue;
        }

        if (line.contains("=")) {
            QStringList parts = line.split("=");
            if (parts.size() == 2) {
                QString key = parts[0].trimmed();
                QString value = parts[1].trimmed();

                if (key == "InternalModelName") {
                    info.internalModel = value;
                } else if (key == "ExtractionMethod") {
                    info.extractionMethod = value;
                } else if (key == "ExtractionStartUtc" || key == "ExtractionEndUtc") {
                    QDateTime dt = parseEwcDateTime(value);
                    if (dt.isValid()) {
                        info.extractionTime = dt;
                    }
                } else if (key == "ProductName") {
                    if (info.brand == "Unknown") {
                        info.brand = value;
                    }
                } else if (key == "ProductVersion") {
                    // Может использоваться как версия Android
                    if (info.androidVersion == "Unknown") {
                        info.androidVersion = value;
                    }
                } else if (key == "DeviceAlias") {
                    if (info.model == "Unknown") {
                        info.model = value;
                    }
                }
            }
        }
    }

    file.close();
    return info;
}

QDateTime DeviceInfoParser::parseEwcDateTime(const QString& dateTimeStr)
{
    if (dateTimeStr.isEmpty()) {
        return QDateTime();
    }

    // Формат: "20240828T165836" -> "2024-08-28T16:58:36"
    if (dateTimeStr.length() == 15) { // YYYYMMDDTHHMMSS
        QString formatted = dateTimeStr;
        formatted.insert(4, "-");
        formatted.insert(7, "-");
        formatted.insert(13, ":");
        formatted.insert(16, ":");

        return QDateTime::fromString(formatted, "yyyy-MM-ddTHH:mm:ss");
    }

    return QDateTime::fromString(dateTimeStr, Qt::ISODate);
}
