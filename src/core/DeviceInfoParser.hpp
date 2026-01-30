#pragma once

#include <QString>
#include <QDateTime>

struct DeviceInfo {
    QString brand;
    QString model;
    QString androidVersion;
    QString firmwareDate;
    QString securityPatchDate;
    QString platform;
    QString imei1;
    QString imei2;
    QString serial;

    // Для информации из device.ewc
    QString internalModel;
    QString extractionMethod;
    QDateTime extractionTime;

    DeviceInfo() :
        brand("Unknown"),
        model("Unknown"),
        androidVersion("Unknown"),
        firmwareDate("Unknown"),
        securityPatchDate("Unknown"),
        platform("Unknown"),
        imei1("Unknown"),
        imei2("Unknown"),
        serial("Unknown"),
        internalModel("Unknown"),
        extractionMethod("Unknown")
    {}

    bool isValid() const {
        return !brand.isEmpty() && brand != "Unknown";
    }

    QString toString() const {
        QString result;
        result += "Brand: " + brand + "\n";
        result += "Model: " + model + "\n";
        result += "Android: " + androidVersion + "\n";
        result += "Platform: " + platform + "\n";
        if (!imei1.isEmpty() && imei1 != "Unknown") {
            result += "IMEI1: " + imei1 + "\n";
        }
        if (!imei2.isEmpty() && imei2 != "Unknown") {
            result += "IMEI2: " + imei2 + "\n";
        }
        if (!serial.isEmpty() && serial != "Unknown") {
            result += "Serial: " + serial + "\n";
        }
        return result;
    }
};

class DeviceInfoParser {
public:
    static DeviceInfo parseFromDirectory(const QString& path);
    static QString detectPlatform(const QString& path);
    static DeviceInfo parseBuildProp(const QString& filePath);
    static DeviceInfo analyzeImageForDeviceInfo(const QString& filePath);
    static bool isLikelyImei(const QString& imei);
    static QString readEwcFile(const QString& filePath);
    static DeviceInfo parseEwcFile(const QString& filePath);

private:
    static QString detectPlatformFromHeader(const QByteArray& header);
    static QString extractFromBuildProp(const QString& filePath, const QString& key);
    static QDateTime parseEwcDateTime(const QString& dateTimeStr);
};
