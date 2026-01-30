#pragma once

#include <QByteArray>
#include <QString>
#include <QCryptographicHash>

#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/sha.h>
#endif

class AndroidCrypto {
public:
    // Типы шифрования Android
    enum CryptoType {
        CRYPTO_UNKNOWN,
        CRYPTO_FDE_AES_CBC,      // Full Disk Encryption
        CRYPTO_FBE_AES_XTS,      // File-Based Encryption
        CRYPTO_MTK_AES_CBC,      // MediaTek специфичный AES-CBC
        CRYPTO_MTK_AES_XTS,      // MediaTek специфичный AES-XTS
        CRYPTO_MTK_SM4_CBC,      // MediaTek SM4-CBC (китайские устройства)
        CRYPTO_MTK_RPMB,         // MediaTek RPMB based
        CRYPTO_LUKS,             // LUKS/dm-crypt
        CRYPTO_AUTO_DETECT       // Автоматическое определение
    };

    struct CryptoParams {
        QByteArray key;
        QByteArray iv;
        QByteArray tweakKey;     // Для XTS режима
        CryptoType type;
        int keySize;
        int ivSize;
        quint64 sectorSize;      // Для блочного шифрования
    };

    // Основные методы
    static QByteArray decryptData(const QByteArray& encryptedData,
                                  const CryptoParams& params,
                                  quint64 sectorOffset = 0);

    // Метод для брутфорса паролей
    static bool tryDecryptWithPassword(const QByteArray& encryptedData,
                                       const QString& password,
                                       const QByteArray& salt,
                                       CryptoType cryptoType);

    // Утилитарные методы
    static QByteArray deriveKeyPbkdf2(const QString& password,
                                      const QByteArray& salt,
                                      int iterations = 10000,
                                      int keyLength = 32);

    static QByteArray calculateHash(const QByteArray& data,
                                    const QString& algorithm = "sha256");

    static bool tryMtkDecryptionImproved(const QByteArray& data,
                                         const QString& password,
                                         const QByteArray& salt);

    static bool isDecryptionSuccessfulImproved(const QByteArray& data);

    static QByteArray deriveKeyPbkdf2Sha256(const QString& password,
                                            const QByteArray& salt,
                                            int iterations = 10000,
                                            int keyLength = 32);

private:
    static QByteArray decryptTest(const QByteArray& data,
                                  const QByteArray& key);

    AndroidCrypto::CryptoParams createMtkParams(const QByteArray& rpmbKey,
                                                               const QByteArray& meId,
                                                               const QString& partitionName);
    static bool isMtkEncrypted(const QByteArray& data);
    static bool tryMtkDecryption(const QByteArray& data,
                                 const QString& password,
                                 const QByteArray& salt,
                                 CryptoType cryptoType);
    static bool tryFdeDecryption(const QByteArray& data,
                                 const QString& password,
                                 const QByteArray& salt);
    static bool tryFbeDecryption(const QByteArray& data,
                                 const QString& password,
                                 const QByteArray& salt);
    static bool tryLuksDecryption(const QByteArray& data,
                                  const QString& password,
                                  const QByteArray& salt);
    static bool tryAutoDecryption(const QByteArray& data,
                                  const QString& password,
                                  const QByteArray& salt);
    static bool tryBruteForceDecryption(const QByteArray& data,
                                        const QString& password,
                                        const QByteArray& salt);

    static bool testUnencryptedData(const QByteArray& data);
    static bool hasAndroidFileMarkers(const QByteArray& data);
    static bool hasFilesystemMarkers(const QByteArray& data);
    static double calculateEntropy(const QByteArray& data);
    static QByteArray mtkSpecialXor(const QByteArray& data, const QByteArray& key);
    static QByteArray simpleLuksTest(const QByteArray& data, const QByteArray& key);
    static bool isDecryptionSuccessful(const QByteArray& decryptedData, float threshold = 30.0f);
    static void saveTestSample(const QByteArray& data, const QString& filename);
};
