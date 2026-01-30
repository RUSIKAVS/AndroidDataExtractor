#pragma once

#include <QString>
#include <QList>
#include <QtGlobal>
#include "AndroidPartition.hpp"

class QFile;
class QByteArray;
class KeyParser;

class PartitionParser
{
public:
    enum ImageType {
        UNKNOWN,
        GPT_IMAGE,
        MBR_IMAGE,
        RAW_PARTITION,
        SPARSE_IMAGE
    };

    PartitionParser();

    ImageType analyzeImage(const QString& filePath);
    QList<AndroidPartition> parseImage(const QString& filePath);
    QList<AndroidPartition> scanDirectory(const QString& dirPath, KeyParser* keyParser = nullptr);

    bool extractPartition(const AndroidPartition& partition,
                          const QString& outputPath,
                          const QByteArray& decryptionKey = QByteArray());

    static QString imageTypeToString(ImageType type);
    QList<AndroidPartition> parseRawDump(const QString& filePath);
    AndroidPartition analyzePartitionWithKeys(const AndroidPartition& partition,
                                              KeyParser* keyParser);

    static QString detectFilesystemType(const QByteArray& header);
    QList<AndroidPartition> extractInPriorityOrder(const QList<AndroidPartition>& partitions);

private:
    QList<AndroidPartition> parseGPT(const QString& filePath);
    QList<AndroidPartition> parseMBR(const QString& filePath);
    QList<AndroidPartition> parseSparseImage(const QString& filePath);

    AndroidPartition analyzeRawFile(const QString& filePath);

    // Новые приватные методы для дешифрования
    bool extractAndDecryptPartition(const AndroidPartition& partition,
                                    const QString& outputPath,
                                    const QByteArray& decryptionKey);

    bool decryptLUKSPartition(QFile& source, QFile& dest,
                              const AndroidPartition& partition,
                              const QByteArray& key);

    bool decryptMTKPartition(QFile& source, QFile& dest,
                             const AndroidPartition& partition,
                             const QByteArray& key);

    bool decryptFBEPartition(QFile& source, QFile& dest,
                             const AndroidPartition& partition,
                             const QByteArray& key);

    bool decryptGeneric(QFile& source, QFile& dest,
                        const AndroidPartition& partition,
                        const QByteArray& key);

    // Функции определения типа шифрования
    QString detectEncryptionType(const QByteArray& header);
    bool isLUKSEncrypted(const QByteArray& header);
    bool isMTKEncrypted(const QByteArray& header);
    bool isFBEEncrypted(const QByteArray& header);
    bool isAndroidFDEEncrypted(const QByteArray& header);

    quint64 readUInt64(QFile& file, qint64 offset);
    quint32 readUInt32(QFile& file, qint64 offset);
    quint16 readUInt16(QFile& file, qint64 offset);

    // Простое копирование без дешифрования
    bool extractPartitionSimple(const AndroidPartition& partition,
                                const QString& outputPath);

    QString guessPartitionName(const QByteArray& header, qint64 offset);
    qint64 determinePartitionSize(QFile& file, qint64 offset, qint64 maxSize);
    QList<AndroidPartition> scanForPartitionsHeuristic(const QString& filePath);
    qint64 estimatePartitionSize(QFile& file, qint64 startOffset, qint64 maxSize);
    QList<AndroidPartition> parseMtkRawDump(const QString& filePath);
    // Новые вспомогательные методы
    double calculateEntropy(const QByteArray& data);
    QString bytesToHex(const QByteArray& data, int maxBytes = 64);
    QString bytesToAscii(const QByteArray& data, int maxBytes = 64);
    bool isLikelyEncrypted(const QByteArray& data);
    qint64 findNextSignature(QFile& file, qint64 startOffset, qint64 maxSize);

};
