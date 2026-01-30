#pragma once

#include <QByteArray>
#include <QString>
#include <QCryptographicHash>

#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#endif

class MtkCrypto {
public:
    enum CryptoType {
        MTK_UNKNOWN,
        MTK_AES_CBC,            // AES-CBC (старые устройства)
        MTK_AES_XTS,            // AES-XTS (новые устройства)
        MTK_SM4_CBC,            // SM4-CBC (китайские устройства)
        MTK_RPMB_KEY,           // Ключ из RPMB
        MTK_FDE_KEY,            // Ключ FDE
        MTK_USER_PASSWORD       // Пользовательский пароль
    };

    struct CryptoParams {
        CryptoType type;
        QByteArray key;
        QByteArray iv;
        QByteArray tweakKey;    // Для XTS режима
        int keySize;
        int ivSize;
        quint64 sectorSize;
        QByteArray additionalData; // Дополнительные данные
    };

    // Основные методы
    static QByteArray decryptData(const QByteArray& encryptedData,
                                  const CryptoParams& params,
                                  quint64 sectorOffset = 0);

    // Методы для брутфорса
    static bool tryDecryptWithPassword(const QByteArray& encryptedData,
                                       const QString& password,
                                       const QByteArray& salt,
                                       const QString& partitionName);

    // Деривация ключей для MTK
    static QByteArray deriveKeyFromRpmb(const QByteArray& rpmbKey,
                                        const QByteArray& meId,
                                        const QString& partitionName);

    static QByteArray deriveKeyFromFde(const QByteArray& fdeKey,
                                       const QByteArray& meId);

    static QByteArray deriveKeyFromPassword(const QString& password,
                                            const QByteArray& salt,
                                            const QString& partitionName,
                                            int iterations = 10000);

    // Определение типа шифрования MTK
    static CryptoType detectMtkEncryption(const QByteArray& header,
                                          const QString& partitionName);

    // Создание параметров для MTK
    static CryptoParams createMtkParams(CryptoType type,
                                        const QByteArray& key,
                                        const QByteArray& meId = QByteArray());

    // Проверка корректности дешифрования
    static bool isDecryptionSuccessful(const QByteArray& decryptedData,
                                       float threshold = 30.0f);

    // Утилитарные методы
    static QByteArray generateMtkIv(const QByteArray& meId,
                                    const QString& partitionName);

    static QByteArray extractMeIdFromHeader(const QByteArray& header);
    static QByteArray extractRpmbKeyFromHeader(const QByteArray& header);

    static QByteArray scryptDerive(const QString& password,
                                   const QByteArray& salt,
                                   int N = 16384, int r = 8, int p = 1,
                                   int keyLength = 32);

private:
#ifdef WITH_OPENSSL
    static QByteArray decryptAesCbc(const QByteArray& data,
                                    const QByteArray& key,
                                    const QByteArray& iv);

    static QByteArray decryptAesXts(const QByteArray& data,
                                    const QByteArray& key,
                                    const QByteArray& tweakKey,
                                    quint64 sector,
                                    int sectorSize = 512);

    static QByteArray decryptSm4Cbc(const QByteArray& data,
                                    const QByteArray& key,
                                    const QByteArray& iv);
#endif

    // Fallback методы без OpenSSL
    static QByteArray simpleAesCbcDecrypt(const QByteArray& data,
                                          const QByteArray& key,
                                          const QByteArray& iv);

    static QByteArray mtkXorDecrypt(const QByteArray& data,
                                    const QByteArray& key,
                                    const QByteArray& meId);

    // Вспомогательные методы
    static QByteArray hmacSha256(const QByteArray& key,
                                 const QByteArray& data);

    static QByteArray pbkdf2Sha1(const QString& password,
                                 const QByteArray& salt,
                                 int iterations,
                                 int keyLength);


};
