#ifndef ANDROIDEXTRACTOR_H
#define ANDROIDEXTRACTOR_H

#include <QObject>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QVector>
#include <QUuid>
#include <QByteArray>
#include <QDataStream>
#include <QElapsedTimer>
#include <QDebug>
#include <QThread>
#include <mutex>
#include <memory>

class AndroidExtractor : public QObject
{
    Q_OBJECT

public:
    struct FileInfo {
        QString fileName;
        QString fullPath;
        qint64 size = 0;
        QDateTime created;
        QDateTime modified;
        QString partitionTableType;
        QString fileSystem;
        QMap<QString, QString> additionalProperties;
        bool isDiskDump = false;
        bool hasGPT = false;
        bool hasMBR = false;
        qint64 sectorSize = 512;
        qint64 sectorCount = 0;
        int partitionNumber = 0;
        qint64 offset = 0;
        qint64 partitionSize = 0;
        QString partitionName;
        QString partitionType;
        QUuid partitionGuid;
        QUuid partitionTypeGuid;
        bool isEncrypted = false;
        QString encryptionType;
        QString debugInfo;

        bool isValid() const { return !fileName.isEmpty() && size > 0; }
    };

    struct FileSystemEntry {
        QString name;
        QString path;
        qint64 size = 0;
        bool isDirectory = false;
        QDateTime modified;
        QString permissions;
        QString owner;
        QString group;
    };

    class RawAccess {
    public:
        virtual ~RawAccess() = default;
        virtual QByteArray readData(qint64 offset, qint64 size) = 0;
        virtual bool writeData(qint64 offset, const QByteArray& data) = 0;
        virtual qint64 size() const = 0;
        virtual bool isOpen() const = 0;
        virtual void close() = 0;
    };

    explicit AndroidExtractor(QObject* parent = nullptr);
    ~AndroidExtractor();

    QList<FileInfo> scanDirectory(const QString& directoryPath);
    QList<FileInfo> analyzeDiskDump(const QString& filePath);
    QList<FileSystemEntry> analyzePartition(const QString& filePath, qint64 offset);

    std::shared_ptr<RawAccess> getRawAccess(const QString& filePath, qint64 offset = 0);

    FileInfo getFileInfo(const QString& filePath);

signals:
    void scanProgress(int percent);
    void fileAnalyzed(const FileInfo& fileInfo);
    void errorOccurred(const QString& error);

private:
    struct GPTPartitionEntry {
        QUuid typeGuid;
        QUuid partitionGuid;
        quint64 firstLBA;
        quint64 lastLBA;
        quint64 attributes;
        QString name;
    };

    struct MBRPartition {
        quint8 status;
        quint8 chsStart[3];
        quint8 type;
        quint8 chsEnd[3];
        quint32 lbaStart;
        quint32 sectorCount;
    };

    FileInfo analyzeFile(const QString& filePath);
    bool isDiskDumpFile(const QByteArray& header, qint64 fileSize);
    bool detectGPT(const QByteArray& data, qint64 offset, FileInfo& info);
    bool detectMBR(const QByteArray& data, qint64 offset, FileInfo& info);
    QString detectFileSystem(const QByteArray& data, qint64 offset);
    QString detectEncryption(const QByteArray& data, qint64 offset);
    QList<GPTPartitionEntry> readGPTPartitions(QFile& file, qint64 gptOffset);
    QList<MBRPartition> readMBRPartitions(const QByteArray& data);
    QString guidToString(const QUuid& guid);
    QString getGuidDescription(const QUuid& guid);
    QString cleanPartitionName(const QString& name);

    QMap<QUuid, QString> initializeGuidMap();
    QMap<QUuid, QString> guidDescriptions;

    class FileRawAccess : public RawAccess {
    public:
        FileRawAccess(const QString& path, qint64 baseOffset = 0);
        ~FileRawAccess();

        QByteArray readData(qint64 offset, qint64 size) override;
        bool writeData(qint64 offset, const QByteArray& data) override;
        qint64 size() const override;
        bool isOpen() const override;
        void close() override;

    private:
        QFile file;
        qint64 baseOffset;
        mutable std::mutex mutex;



    };
};

// Отдельный класс Extractor для удобства использования
class Extractor : public QObject
{
    Q_OBJECT

public:
    explicit Extractor(QObject* parent = nullptr) : QObject(parent) {}

    QList<AndroidExtractor::FileInfo> scanDirectory(const QString& directoryPath) {
        AndroidExtractor extractor;
        return extractor.scanDirectory(directoryPath);
    }

    QList<AndroidExtractor::FileInfo> analyzeDiskDump(const QString& filePath) {
        AndroidExtractor extractor;
        return extractor.analyzeDiskDump(filePath);
    }

    QList<AndroidExtractor::FileSystemEntry> analyzePartition(const QString& filePath, qint64 offset) {
        AndroidExtractor extractor;
        return extractor.analyzePartition(filePath, offset);
    }

    std::shared_ptr<AndroidExtractor::RawAccess> getRawAccess(const QString& filePath, qint64 offset = 0) {
        AndroidExtractor extractor;
        return extractor.getRawAccess(filePath, offset);
    }

    AndroidExtractor::FileInfo getFileInfo(const QString& filePath) {
        AndroidExtractor extractor;
        return extractor.getFileInfo(filePath);
    }

signals:
    void scanProgress(int percent);
};

#endif // ANDROIDEXTRACTOR_H
