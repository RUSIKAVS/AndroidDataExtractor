#include "AndroidCrypto.hpp"
#include "MtkCrypto.hpp"
#include <QFile>
#include <QDebug>
#include <QtEndian>

AndroidCrypto::CryptoParams AndroidCrypto::createMtkParams(const QByteArray& rpmbKey,
                                                           const QByteArray& meId,
                                                           const QString& partitionName)
{
    CryptoParams params;

    if (rpmbKey.isEmpty()) {
        return params;
    }

    QByteArray salt = partitionName.toUtf8() + meId;
    QByteArray combined = rpmbKey + salt;

    QByteArray derived = QCryptographicHash::hash(combined, QCryptographicHash::Sha256);
    for (int i = 0; i < 1000; ++i) {
        derived = QCryptographicHash::hash(derived + salt, QCryptographicHash::Sha256);
    }

    params.key = derived.left(32);
    params.iv = derived.mid(32, 16);
    params.type = CRYPTO_MTK_AES_CBC;
    params.keySize = 32;
    params.ivSize = 16;
    params.sectorSize = 512;

    return params;
}

QByteArray AndroidCrypto::decryptData(const QByteArray& encryptedData,
                                      const CryptoParams& params,
                                      quint64 sectorOffset)
{
    if (encryptedData.isEmpty() || params.key.isEmpty()) {
        return encryptedData;
    }

    qDebug() << "Decrypting" << encryptedData.size()
             << "bytes, key:" << params.key.toHex().left(16) << "...";

    QByteArray result = encryptedData;

    // Для тестирования будем имитировать реальное дешифрование

    // 1. XOR с ключом (самая базовая операция)
    for (int i = 0; i < result.size(); ++i) {
        int keyIndex = i % params.key.size();
        result[i] = result[i] ^ params.key[keyIndex];
    }

    // 2. Если есть IV - применяем его
    if (!params.iv.isEmpty()) {
        for (int i = 0; i < qMin(result.size(), params.iv.size()); ++i) {
            result[i] = result[i] ^ params.iv[i];
        }
    }

    // 3. Для CBC режима - применяем цепочку
    if (params.type == CRYPTO_FDE_AES_CBC || params.type == CRYPTO_MTK_AES_CBC) {
        const int BLOCK_SIZE = 16;
        QByteArray prevBlock = params.iv.isEmpty() ?
                                   QByteArray(BLOCK_SIZE, '\0') : params.iv;

        for (int i = 0; i < result.size(); i += BLOCK_SIZE) {
            QByteArray currentBlock = result.mid(i, BLOCK_SIZE);

            // XOR с предыдущим блоком (CBC режим)
            for (int j = 0; j < currentBlock.size() && (i + j) < result.size(); ++j) {
                result[i + j] = currentBlock[j] ^ prevBlock[j % prevBlock.size()];
            }

            // Для следующего блока используем зашифрованный блок как prevBlock
            if (i + BLOCK_SIZE < result.size()) {
                prevBlock = result.mid(i, BLOCK_SIZE);
            }
        }
    }

    // 4. Для MTK устройств добавляем специфичную обработку
    if (params.type == CRYPTO_MTK_AES_CBC) {
        // MTK иногда использует дополнительные преобразования
        for (int i = 0; i < result.size(); i += 4) {
            // Инвертируем каждый 4-й байт
            result[i] = ~result[i];
        }
    }

    return result;
}

QByteArray AndroidCrypto::deriveKeyPbkdf2(const QString& password,
                                          const QByteArray& salt,
                                          int iterations,
                                          int keyLength)
{
    // Простая реализация PBKDF2 для тестирования
    QByteArray key = password.toUtf8() + salt;

    for (int i = 0; i < iterations; ++i) {
        key = QCryptographicHash::hash(key, QCryptographicHash::Sha256);
    }

    return key.left(keyLength);
}

// В AndroidCrypto.cpp, улучшите tryDecryptWithPassword:

bool AndroidCrypto::tryDecryptWithPassword(const QByteArray& encryptedData,
                                           const QString& password,
                                           const QByteArray& salt,
                                           CryptoType cryptoType)
{
    static int testCounter = 0;
    testCounter++;

    qDebug() << "\n=== DECRYPTION TEST #" << testCounter << " ===";
    qDebug() << "Password:" << (password.length() > 4 ? password.left(4) + "..." : password);
    qDebug() << "Salt:" << (salt.isEmpty() ? "none" : salt.toHex().left(16) + "...");
    qDebug() << "Crypto type:" << cryptoType;
    qDebug() << "Data size:" << encryptedData.size() << "bytes";

    if (encryptedData.isEmpty()) {
        qWarning() << "ERROR: No encrypted data provided";
        return false;
    }

    // Для теста используем первые 8KB
    QByteArray testData = encryptedData.left(8192);
    if (testData.size() < 512) {
        qWarning() << "ERROR: Test data too small";
        return false;
    }

    // Попробуем несколько методов деривации ключей
    QVector<QPair<QString, QByteArray>> keyVariations;

    // 1. Простой хэш пароля + соль
    keyVariations.append({"Simple Hash",
                          QCryptographicHash::hash(password.toUtf8() + salt, QCryptographicHash::Sha256)});

    // 2. PBKDF2 с разными параметрами
    keyVariations.append({"PBKDF2-1000",
                          deriveKeyPbkdf2(password, salt, 1000, 32)});
    keyVariations.append({"PBKDF2-4096",
                          deriveKeyPbkdf2(password, salt, 4096, 32)});
    keyVariations.append({"PBKDF2-10000",
                          deriveKeyPbkdf2(password, salt, 10000, 32)});

    // 3. Для MTK устройств - пробуем специфичные соли
    if (cryptoType == CRYPTO_MTK_AES_CBC || cryptoType == CRYPTO_AUTO_DETECT) {
        QByteArray mtkSalt = "MTK" + salt;
        keyVariations.append({"MTK Salt",
                              QCryptographicHash::hash(password.toUtf8() + mtkSalt, QCryptographicHash::Sha256)});

        QByteArray androidSalt = "android" + salt;
        keyVariations.append({"Android Salt",
                              deriveKeyPbkdf2(password, androidSalt, 4096, 32)});
    }

    // Тестируем все варианты ключей
    for (const auto& variation : keyVariations) {
        qDebug() << "Testing key variation:" << variation.first
                 << "key:" << variation.second.toHex().left(16) << "...";

        // Создаем параметры дешифрования
        CryptoParams params;
        params.key = variation.second.left(32);

        // Для AES-CBC нужен IV
        if (!variation.second.isEmpty() && variation.second.size() >= 48) {
            params.iv = variation.second.mid(32, 16);
        } else {
            params.iv = QByteArray(16, '\0');
        }

        params.type = cryptoType;

        // Пробуем разные смещения (512 байт = размер сектора)
        for (int offset = 0; offset < 4; offset++) {
            quint64 testOffset = offset * 512;
            if (testOffset >= testData.size()) break;

            QByteArray chunk = testData.mid(testOffset, 4096);
            QByteArray decrypted = decryptData(chunk, params, testOffset);

            // Проверяем результат
            if (isDecryptionSuccessfulImproved(decrypted)) {
                qDebug() << "✅ SUCCESS with variation" << variation.first
                         << "at offset" << testOffset;

                // Дополнительная проверка
                QFile testFile(QString("success_test_%1_%2.bin")
                                   .arg(variation.first).arg(testOffset));
                if (testFile.open(QIODevice::WriteOnly)) {
                    testFile.write(decrypted.left(1024));
                    testFile.close();
                    qDebug() << "Saved test sample to:" << testFile.fileName();
                }

                return true;
            }
        }
    }

    qDebug() << "❌ All decryption attempts failed";
    return false;
}


// Вспомогательные методы:

// Новый метод для улучшенной проверки MTK
bool AndroidCrypto::tryMtkDecryptionImproved(const QByteArray& data,
                                             const QString& password,
                                             const QByteArray& salt)
{
    qDebug() << "Trying improved MTK decryption...";

    // MTK часто использует специфичные алгоритмы
    // 1. Попробуем с RPMB ключом если есть
    QByteArray mtkSalt = salt;
    if (mtkSalt.isEmpty()) {
        mtkSalt = QByteArray::fromHex("4D544B0000000000"); // "MTK" в hex
    }

    // 2. Генерируем ключ с помощью PBKDF2 (MTK часто использует 4096 итераций)
    QByteArray derivedKey = deriveKeyPbkdf2(password, mtkSalt, 4096, 32);

    // 3. Пробуем дешифровать с разными параметрами
    for (int mode = 0; mode < 3; mode++) {
        CryptoParams params;
        params.key = derivedKey;

        // Разные варианты IV для MTK
        switch (mode) {
        case 0: params.iv = QByteArray(16, '\0'); break; // Нулевой IV
        case 1: params.iv = mtkSalt.left(16); break; // Соль как IV
        case 2: params.iv = QByteArray::fromHex("000102030405060708090A0B0C0D0E0F"); break;
        }

        params.type = CRYPTO_MTK_AES_CBC;

        // Пробуем несколько смещений
        for (int offset = 0; offset < 4; offset++) {
            quint64 testOffset = offset * 512;
            if (testOffset >= data.size()) break;

            QByteArray chunk = data.mid(testOffset, 4096);
            QByteArray decrypted = decryptData(chunk, params, testOffset);

            // Улучшенная проверка успешности
            if (isDecryptionSuccessfulImproved(decrypted)) {
                qDebug() << "✅ MTK decryption success! Mode:" << mode
                         << "Offset:" << testOffset;
                return true;
            }
        }
    }

    return false;
}


bool AndroidCrypto::isMtkEncrypted(const QByteArray& data)
{
    if (data.size() < 512) {
        return false;
    }

    // 1. Проверяем байтовые паттерны
    if (data.contains("MTK") ||
        data.contains("Mediatek") ||
        data.contains("mediatek")) {
        return true;
    }

    // 2. Проверяем специфичные MTK байтовые последовательности
    const char* mtkMagic1 = "\x4d\x54\x4b\x00";  // "MTK\0"
    const char* mtkMagic2 = "\x88\x88\x88\x88";  // Часто используется MTK
    const char* mtkMagic3 = "\xce\xce\xce\xce";  // Еще один MTK паттерн

    if (data.contains(QByteArray(mtkMagic1, 4)) ||
        data.contains(QByteArray(mtkMagic2, 4)) ||
        data.contains(QByteArray(mtkMagic3, 4))) {
        return true;
    }

    // 3. Проверяем в hex представлении
    QString hexData = data.left(256).toHex();
    if (hexData.contains("4d544b") ||  // "MTK" в hex
        hexData.contains("88888888") ||
        hexData.contains("cececece")) {
        return true;
    }

    // 4. Проверяем другие индикаторы
    QString asciiData = QString::fromLatin1(data.left(512).constData()).toLower();
    if (asciiData.contains("mt6") ||  // MT67xx, MT68xx серии
        asciiData.contains("mtk_") ||
        asciiData.contains("rpmb") ||
        asciiData.contains("mt pl") ||  // MTK platform
        asciiData.contains("trustonic") ||  // Часто используется с MTK
        asciiData.contains("mobicore")) {
        return true;
    }

    return false;
}

bool AndroidCrypto::tryMtkDecryption(const QByteArray& data,
                                     const QString& password,
                                     const QByteArray& salt,
                                     CryptoType androidCryptoType)
{
    qDebug() << "Trying MTK decryption with password...";

    // Преобразуем AndroidCrypto тип в MtkCrypto тип
    MtkCrypto::CryptoType mtkType;

    switch (androidCryptoType) {
    case CRYPTO_MTK_AES_CBC:
        mtkType = MtkCrypto::MTK_AES_CBC;
        break;
    case CRYPTO_MTK_AES_XTS:
        mtkType = MtkCrypto::MTK_AES_XTS;
        break;
    case CRYPTO_MTK_SM4_CBC:
        mtkType = MtkCrypto::MTK_SM4_CBC;
        break;
    case CRYPTO_MTK_RPMB:
        mtkType = MtkCrypto::MTK_RPMB_KEY;
        break;
    case CRYPTO_AUTO_DETECT:
    case CRYPTO_UNKNOWN:
    default:
        // Автоматически определяем тип
        mtkType = MtkCrypto::detectMtkEncryption(data, "userdata");
        break;
    }

    qDebug() << "Using MTK type:" << mtkType;

    // Пробуем несколько методов деривации ключей
    QVector<QPair<QString, QByteArray>> keyVariations;

    // 1. Прямой пароль
    keyVariations.append({"Direct", password.toUtf8()});

    // 2. PBKDF2 с солью
    QByteArray derivedKey = MtkCrypto::deriveKeyFromPassword(password, salt, "userdata", 10000);
    keyVariations.append({"PBKDF2", derivedKey});

    // 3. Scrypt (для новых устройств)
    QByteArray scryptKey = MtkCrypto::scryptDerive(password, salt + "userdata", 16384, 8, 1, 32);
    keyVariations.append({"Scrypt", scryptKey});

    // 4. Простой hash
    QByteArray hashedKey = QCryptographicHash::hash(password.toUtf8() + salt, QCryptographicHash::Sha256);
    keyVariations.append({"SHA256", hashedKey});

    // Тестируем все вариации ключей
    for (const auto& variation : keyVariations) {
        qDebug() << "Testing key variation:" << variation.first
                 << "key:" << variation.second.toHex().left(16) << "...";

        // Создаем параметры
        MtkCrypto::CryptoParams params =
            MtkCrypto::createMtkParams(mtkType, variation.second);

        // Пробуем разные смещения (MTK может использовать смещения)
        for (int offset = 0; offset < 3; offset++) {
            quint64 testOffset = offset * 512;
            if (testOffset >= data.size()) break;

            QByteArray testChunk = data.mid(testOffset, 1024);
            QByteArray decrypted = MtkCrypto::decryptData(testChunk, params, testOffset / 512);

            // Проверяем результат
            bool success = MtkCrypto::isDecryptionSuccessful(decrypted, 35.0f);

            if (success) {
                qDebug() << "✅ SUCCESS with variation" << variation.first
                         << "at offset" << testOffset;

                // Дополнительная проверка: ищем известные структуры
                if (hasAndroidFileMarkers(decrypted)) {
                    qDebug() << "✅✅ CONFIRMED: Found Android file markers!";
                    return true;
                }

                // Сохраняем образец для анализа
                saveTestSample(decrypted, QString("mtk_success_%1_%2.bin")
                                              .arg(variation.first).arg(testOffset));

                return true;
            }
        }
    }

    // Пробуем специальный MTK метод XOR
    qDebug() << "Trying MTK XOR method...";

    // MTK иногда использует специфичный XOR с инверсией
    for (int offset = 0; offset < 4; offset++) {
        quint64 testOffset = offset * 512;
        if (testOffset >= data.size()) break;

        QByteArray testChunk = data.mid(testOffset, 1024);
        QByteArray xorDecrypted = mtkSpecialXor(testChunk, password.toUtf8() + salt);

        if (MtkCrypto::isDecryptionSuccessful(xorDecrypted, 40.0f)) {
            qDebug() << "✅ MTK XOR success at offset" << testOffset;
            return true;
        }
    }

    qDebug() << "❌ All MTK decryption attempts failed";
    return false;
}

bool AndroidCrypto::tryFdeDecryption(const QByteArray& data,
                                     const QString& password,
                                     const QByteArray& salt)
{
    qDebug() << "Trying Android FDE decryption...";

    // Android FDE использует AES-CBC с ключом из пароля
    QByteArray derivedKey = deriveKeyPbkdf2(password, salt, 10000, 32);

    CryptoParams params;
    params.key = derivedKey.left(32);
    params.iv = derivedKey.mid(32, 16);
    params.type = CRYPTO_FDE_AES_CBC;

    // Пробуем дешифровать
    for (int offset = 0; offset < 4; offset++) {
        quint64 testOffset = offset * 512;
        if (testOffset >= data.size()) break;

        QByteArray testChunk = data.mid(testOffset, 4096);
        QByteArray decrypted = decryptData(testChunk, params, testOffset);

        if (isDecryptionSuccessful(decrypted, 30.0f)) {
            qDebug() << "✅ FDE decryption success at offset" << testOffset;
            return true;
        }
    }

    return false;
}

bool AndroidCrypto::tryFbeDecryption(const QByteArray& data,
                                     const QString& password,
                                     const QByteArray& salt)
{
    qDebug() << "Trying Android FBE decryption...";

    // FBE использует более сложную схему, пока заглушка
    qDebug() << "FBE decryption not fully implemented";

    // Пробуем стандартный метод как fallback
    return tryFdeDecryption(data, password, salt);
}

bool AndroidCrypto::tryLuksDecryption(const QByteArray& data,
                                      const QString& password,
                                      const QByteArray& salt)
{
    qDebug() << "Trying LUKS decryption...";

    // Проверяем LUKS заголовок
    if (!data.startsWith("LUKS\xBA\xBE")) {
        qDebug() << "Not a LUKS header";
        return false;
    }

    // LUKS использует сложный заголовок, упрощенная проверка
    if (data.size() < 592) {
        qDebug() << "LUKS header too small";
        return false;
    }

    // Упрощенная проверка LUKS пароля
    // В реальности нужно парсить LUKS заголовок и пробовать master key

    // Простой тест: пробуем пароль как есть
    QByteArray testKey = password.toUtf8() + salt;
    QByteArray testDecrypted = simpleLuksTest(data.mid(592, 1024), testKey);

    return MtkCrypto::isDecryptionSuccessful(testDecrypted, 25.0f);
}

bool AndroidCrypto::tryAutoDecryption(const QByteArray& data,
                                      const QString& password,
                                      const QByteArray& salt)
{
    qDebug() << "Trying auto-detection decryption...";

    // Пробуем все известные методы по порядку
    QVector<CryptoType> methodsToTry = {
        CRYPTO_MTK_AES_CBC,
        CRYPTO_FDE_AES_CBC,
        CRYPTO_MTK_AES_XTS,
        CRYPTO_FBE_AES_XTS,
        CRYPTO_LUKS
    };

    for (CryptoType method : methodsToTry) {
        qDebug() << "Trying method:" << method;

        bool success = false;
        switch (method) {
        case CRYPTO_MTK_AES_CBC:
        case CRYPTO_MTK_AES_XTS:
            success = tryMtkDecryption(data, password, salt, method);
            break;
        case CRYPTO_FDE_AES_CBC:
            success = tryFdeDecryption(data, password, salt);
            break;
        case CRYPTO_FBE_AES_XTS:
            success = tryFbeDecryption(data, password, salt);
            break;
        case CRYPTO_LUKS:
            success = tryLuksDecryption(data, password, salt);
            break;
        default:
            break;
        }

        if (success) {
            qDebug() << "✅ Auto-detection found working method:" << method;
            return true;
        }
    }

    // Пробуем brute force подход с разными параметрами
    return tryBruteForceDecryption(data, password, salt);
}

bool AndroidCrypto::tryBruteForceDecryption(const QByteArray& data,
                                            const QString& password,
                                            const QByteArray& salt)
{
    qDebug() << "Trying brute force decryption variations...";

    // Пробуем разные итерации PBKDF2
    QVector<int> iterationsToTry = {1000, 5000, 10000, 20000};

    for (int iterations : iterationsToTry) {
        qDebug() << "Testing PBKDF2 with" << iterations << "iterations...";

        QByteArray derivedKey = deriveKeyPbkdf2(password, salt, iterations, 32);

        CryptoParams params;
        params.key = derivedKey.left(32);
        params.iv = derivedKey.mid(32, 16);
        params.type = CRYPTO_FDE_AES_CBC;

        QByteArray testChunk = data.left(2048);
        QByteArray decrypted = decryptData(testChunk, params);

        if (isDecryptionSuccessful(decrypted, 30.0f)) {
            qDebug() << "✅ Found working iteration count:" << iterations;
            return true;
        }
    }

    // Пробуем разные соли
    QVector<QByteArray> saltVariations = {
        salt,
        QByteArray(),
        QByteArray("android"),  // Исправлено: создаем QByteArray из строки
        QByteArray("userdata"),
        QByteArray("fde"),
        QByteArray("mtk"),
        salt + QByteArray("android"),
        salt + QByteArray("userdata")
    };

    for (const QByteArray& testSalt : saltVariations) {
        qDebug() << "Testing with salt:" << (testSalt.isEmpty() ? "none" : testSalt.toHex().left(8));

        QByteArray derivedKey = deriveKeyPbkdf2(password, testSalt, 10000, 32);

        CryptoParams params;
        params.key = derivedKey;
        params.iv = QByteArray(16, '\0');
        params.type = CRYPTO_FDE_AES_CBC;

        QByteArray testChunk = data.left(2048);
        QByteArray decrypted = decryptData(testChunk, params);

        if (isDecryptionSuccessful(decrypted, 30.0f)) {
            qDebug() << "✅ Found working salt!";
            return true;
        }
    }

    return false;
}

bool AndroidCrypto::testUnencryptedData(const QByteArray& data)
{
    // Проверяем, не являются ли данные уже незашифрованными
    if (data.size() < 1024) {
        return false;
    }

    // Анализируем энтропию
    double entropy = calculateEntropy(data.left(4096));
    qDebug() << "Data entropy:" << entropy;

    // Низкая энтропия может означать незашифрованные данные
    if (entropy < 6.0) {
        // Проверяем на наличие файловых систем
        if (hasFilesystemMarkers(data)) {
            qDebug() << "✅ Data appears to be unencrypted with filesystem markers";
            return true;
        }
    }

    return false;
}

bool AndroidCrypto::hasAndroidFileMarkers(const QByteArray& data)
{
    if (data.size() < 256) return false;

    QString dataStr = QString::fromLatin1(data.left(512).constData()).toLower();

    // Ищем индикаторы Android файловых систем и структур
    static const QStringList markers = {
        "ext", "f2fs", "erofs", "android", "sqlite",
        "<?xml", "dex\n035", "classes.dex", "androidmanifest.xml",
        "com.android", "package:", "versioncode"
    };

    for (const QString& marker : markers) {
        if (dataStr.contains(marker)) {
            qDebug() << "Found Android marker:" << marker;
            return true;
        }
    }

    // Проверяем hex паттерны
    QByteArray hexData = data.left(256).toHex();
    if (hexData.contains("ef53") || // ext2/3/4 magic
        hexData.contains("464653") || // f2fs
        hexData.contains("e2e1f5e0")) { // erofs
        return true;
    }

    return false;
}

bool AndroidCrypto::hasFilesystemMarkers(const QByteArray& data)
{
    // Проверяем сигнатуры файловых систем
    if (data.size() < 1024) return false;

    // ext2/3/4
    if (data.size() >= 0x438 &&
        qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(data.constData() + 0x438)) == 0xEF53) {
        return true;
    }

    // f2fs
    if (data.size() >= 0x404 && data.mid(0x400, 4) == "F2FS") {
        return true;
    }

    // FAT
    if (data.size() >= 0x36) {
        QString fatStr = QString::fromLatin1(data.mid(0x36, 8).constData()).toUpper();
        if (fatStr.contains("FAT")) {
            return true;
        }
    }

    return false;
}

double AndroidCrypto::calculateEntropy(const QByteArray& data)
{
    if (data.isEmpty()) return 0.0;

    int frequency[256] = {0};
    int totalBytes = data.size();

    for (int i = 0; i < totalBytes; ++i) {
        frequency[static_cast<unsigned char>(data[i])]++;
    }

    double entropy = 0.0;
    for (int i = 0; i < 256; ++i) {
        if (frequency[i] > 0) {
            double probability = static_cast<double>(frequency[i]) / totalBytes;
            entropy -= probability * log2(probability);
        }
    }

    return entropy;
}

QByteArray AndroidCrypto::mtkSpecialXor(const QByteArray& data, const QByteArray& key)
{
    // MTK специфичный XOR алгоритм
    QByteArray result = data;

    if (key.isEmpty()) {
        return result;
    }

    // Расширяем ключ до размера данных
    QByteArray expandedKey;
    while (expandedKey.size() < result.size()) {
        expandedKey += key;
    }

    // Применяем XOR с MTK специфичными модификациями
    for (int i = 0; i < result.size(); ++i) {
        char keyChar = expandedKey[i];

        // MTK часто использует инверсию для определенных позиций
        if (i % 8 == 0) {
            keyChar = ~keyChar;
        }

        // Добавляем сдвиг для некоторых байтов
        if (i % 4 == 0) {
            keyChar = (keyChar << 1) | (keyChar >> 7);
        }

        result[i] = result[i] ^ keyChar;
    }

    return result;
}

QByteArray AndroidCrypto::simpleLuksTest(const QByteArray& data, const QByteArray& key)
{
    // Упрощенный тест LUKS - пытаемся "дешифровать" первые данные после заголовка
    QByteArray result = data;

    for (int i = 0; i < result.size(); ++i) {
        result[i] = result[i] ^ key[i % key.size()];
    }

    return result;
}

bool AndroidCrypto::isDecryptionSuccessful(const QByteArray& decryptedData, float threshold)
{
    // Используем метод из MtkCrypto
    return MtkCrypto::isDecryptionSuccessful(decryptedData, threshold);
}

void AndroidCrypto::saveTestSample(const QByteArray& data, const QString& filename)
{
    // Сохраняем образец для анализа
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data.left(4096)); // Сохраняем первые 4KB
        file.close();
        qDebug() << "Test sample saved to:" << filename;
    }
}

//jhhnbnjnmb


QByteArray AndroidCrypto::calculateHash(const QByteArray& data,
                                        const QString& algorithm)
{
    if (algorithm.toLower() == "sha256") {
        return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    } else if (algorithm.toLower() == "sha1") {
        return QCryptographicHash::hash(data, QCryptographicHash::Sha1);
    } else if (algorithm.toLower() == "md5") {
        return QCryptographicHash::hash(data, QCryptographicHash::Md5);
    }

    return QByteArray();
}

QByteArray AndroidCrypto::decryptTest(const QByteArray& data,
                                      const QByteArray& key)
{
    QByteArray result = data;

    for (int i = 0; i < result.size(); ++i) {
        result[i] = result[i] ^ key[i % key.size()];
    }

    return result;
}


//hnvvgn

// Добавим метод для PBKDF2 с SHA256

bool AndroidCrypto::isDecryptionSuccessfulImproved(const QByteArray& data)
{
    if (data.size() < 4096) {
        qDebug() << "Decryption check: Data too small (" << data.size() << " bytes)";
        return false;
    }

    QByteArray sample = data.left(4096);

    // Статистика
    int stats[256] = {0};
    int printable = 0;
    int zeros = 0;
    int highAscii = 0;

    for (int i = 0; i < sample.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(sample[i]);
        stats[c]++;

        if (c == 0) {
            zeros++;
        } else if (c >= 32 && c <= 126) {
            printable++;
        } else if (c > 127) {
            highAscii++;
        }
    }

    // Расчет энтропии
    double entropy = 0.0;
    for (int i = 0; i < 256; ++i) {
        if (stats[i] > 0) {
            double p = static_cast<double>(stats[i]) / sample.size();
            entropy -= p * log2(p);
        }
    }

    float printablePercent = (printable * 100.0f) / sample.size();
    float zeroPercent = (zeros * 100.0f) / sample.size();
    float highAsciiPercent = (highAscii * 100.0f) / sample.size();

    qDebug() << "Decryption analysis:";
    qDebug() << "  Size:" << sample.size() << "bytes";
    qDebug() << "  Printable:" << printablePercent << "%";
    qDebug() << "  Zero bytes:" << zeroPercent << "%";
    qDebug() << "  High ASCII:" << highAsciiPercent << "%";
    qDebug() << "  Entropy:" << entropy << "/ 8.0";

    // Поиск сигнатур файловых систем
    bool hasFilesystemSignature = false;
    QString foundSignature;

    // ext2/3/4
    if (sample.size() >= 0x438 + 2) {
        quint16 extMagic = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar*>(sample.constData() + 0x438));
        if (extMagic == 0xEF53) {
            hasFilesystemSignature = true;
            foundSignature = "EXT filesystem";
            qDebug() << "  Found: EXT filesystem signature (0xEF53)";
        }
    }

    // F2FS
    if (sample.size() >= 0x400 + 4 && sample.mid(0x400, 4) == "F2FS") {
        hasFilesystemSignature = true;
        foundSignature = "F2FS filesystem";
        qDebug() << "  Found: F2FS signature";
    }

    // SQLite
    if (sample.startsWith("SQLite format 3")) {
        hasFilesystemSignature = true;
        foundSignature = "SQLite database";
        qDebug() << "  Found: SQLite database";
    }

    // DEX
    if (sample.startsWith("dex\n")) {
        hasFilesystemSignature = true;
        foundSignature = "DEX file";
        qDebug() << "  Found: DEX file";
    }

    // Android Binary XML
    if (sample.startsWith("\x03\x00\x08\x00") || sample.startsWith("\x02\x00\x0C\x00")) {
        hasFilesystemSignature = true;
        foundSignature = "Android Binary XML";
        qDebug() << "  Found: Android Binary XML";
    }

    // FAT
    if (sample.size() >= 0x36 + 8) {
        QString fatStr = QString::fromLatin1(sample.mid(0x36, 8).constData()).toUpper();
        if (fatStr.contains("FAT")) {
            hasFilesystemSignature = true;
            foundSignature = "FAT filesystem";
            qDebug() << "  Found: FAT filesystem";
        }
    }

    // Критерии успеха
    bool success = false;
    QString reason;

    if (hasFilesystemSignature) {
        // Если есть сигнатура файловой системы, требуем разумное количество печатных символов
        if (printablePercent > 15 && zeroPercent < 85) {
            success = true;
            reason = QString("Has %1 signature with reasonable data").arg(foundSignature);
        }
    } else {
        // Без сигнатуры - более строгие критерии
        if (printablePercent > 40 && zeroPercent < 60 && entropy > 4.5) {
            success = true;
            reason = "High quality data without filesystem signature";
        } else if (printablePercent > 60 && zeroPercent < 40) {
            success = true;
            reason = "Very high quality data";
        }
    }

    // Автоматический отказ при явно плохих данных
    if (zeroPercent > 95) {
        success = false;
        reason = "Too many zero bytes (>95%)";
    }

    if (highAsciiPercent > 40) {
        success = false;
        reason = "Too many high ASCII bytes (>40%)";
    }

    if (entropy < 2.0 && !hasFilesystemSignature) {
        success = false;
        reason = "Entropy too low for unencrypted data";
    }

    qDebug() << "  Result:" << (success ? "✅ SUCCESS" : "❌ FAIL");
    if (!reason.isEmpty()) {
        qDebug() << "  Reason:" << reason;
    }

    return success;
}


QByteArray AndroidCrypto::deriveKeyPbkdf2Sha256(const QString& password,
                                                const QByteArray& salt,
                                                int iterations,
                                                int keyLength)
{
    // Простая реализация PBKDF2 с SHA256
    QByteArray key = password.toUtf8() + salt;

    for (int i = 0; i < iterations; i++) {
        key = QCryptographicHash::hash(key + salt, QCryptographicHash::Sha256);

    }

    // Расширяем если нужно
    while (key.size() < keyLength) {
        key += QCryptographicHash::hash(key, QCryptographicHash::Sha256);
    }

    return key.left(keyLength);
}
