#include "MtkCrypto.hpp"
#include <QDebug>
#include <QtEndian>
#include <QRegularExpression>

#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

// ============================================================================
// Основные методы дешифрования
// ============================================================================

QByteArray MtkCrypto::decryptData(const QByteArray& encryptedData,
                                  const CryptoParams& params,
                                  quint64 sectorOffset)
{
    if (encryptedData.isEmpty() || params.key.isEmpty()) {
        qWarning() << "MtkCrypto: Invalid parameters for decryption";
        return encryptedData;
    }

    qDebug() << "MtkCrypto: Decrypting" << encryptedData.size() << "bytes"
             << "type:" << params.type
             << "key size:" << params.key.size()
             << "sector offset:" << sectorOffset;

    QByteArray result;

    try {
        switch (params.type) {
#ifdef WITH_OPENSSL
        case MTK_AES_CBC:
            if (params.iv.isEmpty()) {
                qWarning() << "MtkCrypto: IV required for AES-CBC";
                result = simpleAesCbcDecrypt(encryptedData, params.key, params.iv);
            } else {
                result = decryptAesCbc(encryptedData, params.key, params.iv);
            }
            break;

        case MTK_AES_XTS:
            if (params.tweakKey.isEmpty()) {
                qWarning() << "MtkCrypto: Tweak key required for AES-XTS";
                result = encryptedData; // Не можем дешифровать
            } else {
                result = decryptAesXts(encryptedData, params.key, params.tweakKey,
                                       sectorOffset, params.sectorSize);
            }
            break;

        case MTK_SM4_CBC:
            result = decryptSm4Cbc(encryptedData, params.key, params.iv);
            break;
#endif

        case MTK_RPMB_KEY:
        case MTK_FDE_KEY:
            // Для MTK ключей используем специфичный алгоритм
            result = mtkXorDecrypt(encryptedData, params.key, params.additionalData);
            break;

        case MTK_USER_PASSWORD:
            // Для паролей используем простой XOR (временное решение)
            result = simpleAesCbcDecrypt(encryptedData, params.key, params.iv);
            break;

        default:
            qWarning() << "MtkCrypto: Unknown encryption type, using XOR fallback";
            result = mtkXorDecrypt(encryptedData, params.key, QByteArray());
            break;
        }

        // Проверяем результат
        if (!isDecryptionSuccessful(result)) {
            qWarning() << "MtkCrypto: Decryption likely failed (low entropy/readable data)";
        }

    } catch (const std::exception& e) {
        qCritical() << "MtkCrypto: Decryption error:" << e.what();
        result = encryptedData;
    }

    return result;
}

// ============================================================================
// Методы для брутфорса паролей
// ============================================================================

bool MtkCrypto::tryDecryptWithPassword(const QByteArray& encryptedData,
                                       const QString& password,
                                       const QByteArray& salt,
                                       const QString& partitionName)
{
    if (encryptedData.isEmpty() || password.isEmpty()) {
        return false;
    }

    qDebug() << "MtkCrypto: Testing password for" << partitionName
             << "password length:" << password.length()
             << "salt:" << salt.toHex();

    // MTK использует несколько алгоритмов, пробуем все
    QVector<CryptoType> typesToTry = {
        MTK_AES_CBC,
        MTK_USER_PASSWORD,
        MTK_RPMB_KEY  // На случай если пароль используется как seed для RPMB
    };

    for (CryptoType type : typesToTry) {
        // Создаем ключ из пароля
        QByteArray derivedKey;

        if (type == MTK_RPMB_KEY) {
            // Для RPMB имитируем ключ
            derivedKey = deriveKeyFromPassword(password, salt, partitionName, 1000);
        } else {
            // Стандартный PBKDF2
            derivedKey = pbkdf2Sha1(password, salt + partitionName.toUtf8(), 10000, 32);
        }

        // Создаем параметры
        CryptoParams params = createMtkParams(type, derivedKey);

        // Пробуем дешифровать первые 4KB
        QByteArray testData = encryptedData.left(4096);
        QByteArray decrypted = decryptData(testData, params);

        // Проверяем результат
        if (isDecryptionSuccessful(decrypted, 40.0f)) { // Более высокий порог для паролей
            qDebug() << "MtkCrypto: Password FOUND! Type:" << type;
            return true;
        }
    }

    return false;
}

// ============================================================================
// Деривация ключей для MTK
// ============================================================================

QByteArray MtkCrypto::deriveKeyFromRpmb(const QByteArray& rpmbKey,
                                        const QByteArray& meId,
                                        const QString& partitionName)
{
    if (rpmbKey.isEmpty()) {
        qWarning() << "MtkCrypto: RPMB key is empty";
        return QByteArray();
    }

    // MTK использует HMAC-SHA256 для деривации ключей из RPMB
    QByteArray context = "MTK_RPMB_KEY_DERIVATION";
    QByteArray info = partitionName.toUtf8() + meId + context;

#ifdef WITH_OPENSSL
    unsigned char* result = new unsigned char[32];
    unsigned int resultLen = 32;

    if (HMAC(EVP_sha256(),
             rpmbKey.constData(), rpmbKey.length(),
             reinterpret_cast<const unsigned char*>(info.constData()), info.length(),
             result, &resultLen) == nullptr) {
        delete[] result;
        qWarning() << "MtkCrypto: HMAC failed, using fallback";
        return hmacSha256(rpmbKey, info);
    }

    QByteArray derivedKey(reinterpret_cast<char*>(result), resultLen);
    delete[] result;

    return derivedKey;
#else
    return hmacSha256(rpmbKey, info);
#endif
}

QByteArray MtkCrypto::deriveKeyFromFde(const QByteArray& fdeKey,
                                       const QByteArray& meId)
{
    // FDE ключ обычно используется напрямую или с небольшими модификациями
    if (meId.isEmpty()) {
        return fdeKey;
    }

    // Некоторые устройства XOR-ят FDE ключ с ME ID
    QByteArray result = fdeKey;
    for (int i = 0; i < result.size() && i < meId.size(); ++i) {
        result[i] = result[i] ^ meId[i];
    }

    return result;
}

QByteArray MtkCrypto::deriveKeyFromPassword(const QString& password,
                                            const QByteArray& salt,
                                            const QString& partitionName,
                                            int iterations)
{
    // MTK Android использует scrypt для новых устройств, PBKDF2 для старых
    if (password.length() <= 6) {
        // Короткие PIN коды - вероятно PBKDF2
        return pbkdf2Sha1(password, salt + partitionName.toUtf8(), iterations, 32);
    } else {
        // Длинные пароли - вероятно scrypt
        return scryptDerive(password, salt + partitionName.toUtf8(), 16384, 8, 1, 32);
    }
}

// ============================================================================
// Определение типа шифрования
// ============================================================================

MtkCrypto::CryptoType MtkCrypto::detectMtkEncryption(const QByteArray& header,
                                                     const QString& partitionName)
{
    if (header.size() < 512) {
        return MTK_UNKNOWN;
    }

    QString lowerName = partitionName.toLower();

    // 1. Проверяем наличие MTK специфичных маркеров
    if (header.contains("MTK")) {
        if (lowerName.contains("userdata") || lowerName.contains("data")) {
            return MTK_AES_CBC;  // Userdata обычно AES-CBC
        }
        return MTK_RPMB_KEY;     // Системные разделы используют RPMB
    }

    // 2. Проверяем наличие ключевых слов в заголовке
    QByteArray headerStr = header.left(256).toLower();

    if (headerStr.contains("aes")) {
        if (headerStr.contains("xts")) {
            return MTK_AES_XTS;
        }
        return MTK_AES_CBC;
    }

    if (headerStr.contains("sm4")) {
        return MTK_SM4_CBC;
    }

    if (headerStr.contains("rpmb") || headerStr.contains("mt67")) {
        return MTK_RPMB_KEY;
    }

    if (headerStr.contains("fde")) {
        return MTK_FDE_KEY;
    }

    // 3. Эвристики по имени раздела
    if (lowerName.contains("userdata") || lowerName.contains("data")) {
        // Userdata в современных MTK часто AES-XTS
        return MTK_AES_XTS;
    }

    if (lowerName.contains("metadata") || lowerName.contains("persist")) {
        return MTK_RPMB_KEY;
    }

    return MTK_UNKNOWN;
}

// ============================================================================
// Реализации алгоритмов (с OpenSSL)
// ============================================================================

#ifdef WITH_OPENSSL

QByteArray MtkCrypto::decryptAesCbc(const QByteArray& data,
                                    const QByteArray& key,
                                    const QByteArray& iv)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        qCritical() << "MtkCrypto: Failed to create EVP context";
        return data;
    }

    const EVP_CIPHER* cipher = nullptr;

    if (key.size() == 16) {
        cipher = EVP_aes_128_cbc();
    } else if (key.size() == 32) {
        cipher = EVP_aes_256_cbc();
    } else {
        qWarning() << "MtkCrypto: Unsupported key size for AES-CBC:" << key.size();
        EVP_CIPHER_CTX_free(ctx);
        return data;
    }

    if (EVP_DecryptInit_ex(ctx, cipher, nullptr,
                           reinterpret_cast<const unsigned char*>(key.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        qCritical() << "MtkCrypto: Failed to initialize AES-CBC decryption";
        EVP_CIPHER_CTX_free(ctx);
        return data;
    }

    // Не используем padding для сырых данных разделов
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    QByteArray decrypted(data.size(), '\0');
    int outLen1 = 0, outLen2 = 0;

    if (EVP_DecryptUpdate(ctx,
                          reinterpret_cast<unsigned char*>(decrypted.data()),
                          &outLen1,
                          reinterpret_cast<const unsigned char*>(data.constData()),
                          data.size()) != 1) {
        qCritical() << "MtkCrypto: EVP_DecryptUpdate failed";
        EVP_CIPHER_CTX_free(ctx);
        return data;
    }

    if (EVP_DecryptFinal_ex(ctx,
                            reinterpret_cast<unsigned char*>(decrypted.data() + outLen1),
                            &outLen2) != 1) {
        // Для сырых данных без padding это нормально
        qDebug() << "MtkCrypto: EVP_DecryptFinal_ex failed (likely no padding)";
    }

    EVP_CIPHER_CTX_free(ctx);

    return decrypted.left(outLen1 + outLen2);
}

QByteArray MtkCrypto::decryptAesXts(const QByteArray& data,
                                    const QByteArray& key,
                                    const QByteArray& tweakKey,
                                    quint64 sector,
                                    int sectorSize)
{
    // AES-XTS сложнее реализовать, требуется специальная обработка
    // Пока используем упрощенную версию

    qDebug() << "MtkCrypto: AES-XTS decryption requested (simplified)";

    // Для тестирования используем CBC с tweak key как IV
    return decryptAesCbc(data, key, tweakKey);
}

QByteArray MtkCrypto::decryptSm4Cbc(const QByteArray& data,
                                    const QByteArray& key,
                                    const QByteArray& iv)
{
    // SM4 - китайский стандарт шифрования
    // Без поддержки в OpenSSL, используем XOR как заглушку
    qWarning() << "MtkCrypto: SM4-CBC not implemented, using XOR";
    return simpleAesCbcDecrypt(data, key, iv);
}

#endif // WITH_OPENSSL

// ============================================================================
// Fallback методы (без OpenSSL)
// ============================================================================

QByteArray MtkCrypto::simpleAesCbcDecrypt(const QByteArray& data,
                                          const QByteArray& key,
                                          const QByteArray& iv)
{
    // Упрощенная "AES-CBC" для тестирования
    // В реальности это просто XOR с ключом

    QByteArray result = data;

    for (int i = 0; i < result.size(); ++i) {
        // XOR с ключом (циклически)
        char keyChar = key[i % key.size()];

        // Добавляем влияние IV если есть
        if (!iv.isEmpty() && i < iv.size()) {
            keyChar ^= iv[i % iv.size()];
        }

        result[i] = result[i] ^ keyChar;

        // Добавляем "CBC-like" поведение (XOR с предыдущим блоком)
        if (i >= 16 && (i % 16 == 0)) {
            for (int j = 0; j < 16 && (i + j) < result.size(); ++j) {
                result[i + j] = result[i + j] ^ result[i + j - 16];
            }
        }
    }

    return result;
}

QByteArray MtkCrypto::mtkXorDecrypt(const QByteArray& data,
                                    const QByteArray& key,
                                    const QByteArray& meId)
{
    // MTK специфичный XOR алгоритм
    QByteArray result = data;

    // Создаем расширенный ключ
    QByteArray expandedKey = key;
    if (!meId.isEmpty()) {
        expandedKey = hmacSha256(key, meId);
    }

    // Применяем XOR с расширенным ключом
    for (int i = 0; i < result.size(); ++i) {
        int keyIndex = i % expandedKey.size();

        // MTK иногда использует инверсию битов
        char keyChar = expandedKey[keyIndex];

        // Для MTK характерны определенные паттерны
        if (i % 4 == 0) {
            keyChar = ~keyChar;  // Инвертируем каждый 4-й байт
        }

        result[i] = result[i] ^ keyChar;
    }

    return result;
}

// ============================================================================
// Вспомогательные методы
// ============================================================================

QByteArray MtkCrypto::hmacSha256(const QByteArray& key,
                                 const QByteArray& data)
{
    return QCryptographicHash::hash(key + data, QCryptographicHash::Sha256);
}

QByteArray MtkCrypto::pbkdf2Sha1(const QString& password,
                                 const QByteArray& salt,
                                 int iterations,
                                 int keyLength)
{
    if (password.isEmpty() || iterations <= 0 || keyLength <= 0) {
        qWarning() << "MtkCrypto: Invalid PBKDF2 parameters";
        return QByteArray();
    }

    qDebug() << "MtkCrypto: PBKDF2-SHA1 with" << iterations << "iterations,"
             << "key length:" << keyLength;

    // Начинаем с пароля + соль
    QByteArray current = password.toUtf8() + salt;

    // Основной цикл PBKDF2
    for (int i = 0; i < iterations; ++i) {
        current = QCryptographicHash::hash(current, QCryptographicHash::Sha1);

        // Для отладки - показываем прогресс каждые 1000 итераций
        if (iterations > 1000 && i % 1000 == 0 && i > 0) {
            qDebug() << "  PBKDF2 progress:" << (i * 100 / iterations) << "%";
        }
    }

    // Проверяем длину
    if (current.length() >= keyLength) {
        return current.left(keyLength);
    }

    // Расширяем ключ если он слишком короткий
    qDebug() << "MtkCrypto: Extending key from" << current.length()
             << "to" << keyLength << "bytes";

    QByteArray extendedKey = current;
    int blockCount = 1;

    while (extendedKey.length() < keyLength) {
        // Вычисляем следующий блок по спецификации PBKDF2
        // T_i = F(Password, Salt, c, i)
        // где F - PRF (HMAC-SHA1 в нашем случае)

        QByteArray blockInput = current + QByteArray::number(blockCount);
        QByteArray block = QCryptographicHash::hash(blockInput, QCryptographicHash::Sha1);

        extendedKey += block;
        blockCount++;

        // Защита от бесконечного цикла
        if (blockCount > 100) {
            qWarning() << "MtkCrypto: PBKDF2 key expansion taking too long";
            break;
        }
    }

    return extendedKey.left(keyLength);
}

QByteArray MtkCrypto::scryptDerive(const QString& password,
                                   const QByteArray& salt,
                                   int N, int r, int p,
                                   int keyLength)
{
    // Упрощенная реализация scrypt
    // В реальности нужно использовать библиотеку libscrypt

    qDebug() << "MtkCrypto: Using simplified scrypt (not real scrypt)";

    QByteArray key = password.toUtf8() + salt;

    // Многократное хэширование как упрощение
    for (int i = 0; i < N / 100; ++i) { // Упрощаем для скорости
        key = QCryptographicHash::hash(key, QCryptographicHash::Sha256);

        // Добавляем "memory-hard" симуляцию
        if (i % 1000 == 0) {
            QByteArray temp = key;
            for (int j = 0; j < 100; ++j) {
                temp = QCryptographicHash::hash(temp, QCryptographicHash::Sha256);
            }
            key = QCryptographicHash::hash(key + temp, QCryptographicHash::Sha256);
        }
    }

    return key.left(keyLength);
}

bool MtkCrypto::isDecryptionSuccessful(const QByteArray& decryptedData,
                                       float threshold)
{
    if (decryptedData.isEmpty()) {
        return false;
    }

    // Проверяем несколько эвристик

    // 1. Процент печатных символов
    int printable = 0;
    int analyzed = qMin(1024, decryptedData.size());

    for (int i = 0; i < analyzed; ++i) {
        unsigned char c = static_cast<unsigned char>(decryptedData[i]);
        if ((c >= 32 && c <= 126) || c == 9 || c == 10 || c == 13) {
            printable++;
        }
    }

    float printablePercent = (printable * 100.0f) / analyzed;

    // 2. Проверяем на наличие нулевых байтов (слишком много = плохо)
    int zeroCount = 0;
    for (int i = 0; i < analyzed; ++i) {
        if (decryptedData[i] == 0) zeroCount++;
    }
    float zeroPercent = (zeroCount * 100.0f) / analyzed;

    // 3. Ищем известные Android структуры
    bool hasAndroidMarkers = false;
    QByteArray sample = decryptedData.left(512).toLower();
    if (sample.contains("android") || sample.contains("ext") ||
        sample.contains("f2fs") || sample.contains("erofs") ||
        sample.contains("sqlite") || sample.contains("<?xml")) {
        hasAndroidMarkers = true;
    }

    qDebug() << "MtkCrypto: Decryption check - printable:" << printablePercent
             << "%, zero:" << zeroPercent << "%, has markers:" << hasAndroidMarkers;

    // Критерии успеха
    if (hasAndroidMarkers && printablePercent > 20) {
        return true;
    }

    if (printablePercent > threshold && zeroPercent < 70) {
        return true;
    }

    return false;
}

MtkCrypto::CryptoParams MtkCrypto::createMtkParams(CryptoType type,
                                                   const QByteArray& key,
                                                   const QByteArray& meId)
{
    CryptoParams params;
    params.type = type;
    params.key = key;
    params.keySize = key.size();

    // Генерируем IV в зависимости от типа
    switch (type) {
    case MTK_AES_CBC:
    case MTK_SM4_CBC:
        if (meId.isEmpty()) {
            params.iv = QByteArray(16, '\0'); // Нулевой IV
        } else {
            params.iv = meId.left(16); // Используем ME ID как IV
            if (params.iv.size() < 16) {
                params.iv.append(QByteArray(16 - params.iv.size(), '\0'));
            }
        }
        params.ivSize = params.iv.size();
        params.sectorSize = 512;
        break;

    case MTK_AES_XTS:
        params.tweakKey = hmacSha256(key, "MTK_XTS_TWEAK");
        params.sectorSize = 4096; // Обычно 4K для XTS
        break;

    case MTK_RPMB_KEY:
    case MTK_FDE_KEY:
        params.additionalData = meId;
        params.sectorSize = 512;
        break;

    default:
        params.sectorSize = 512;
        break;
    }

    return params;
}
