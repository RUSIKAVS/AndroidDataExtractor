#pragma once

#include <QString>
#include <QFile>
#include <optional>

#include "Ext4Structs.hpp"

/**
 * @brief Ext4BlockDevice
 *
 * Низкоуровневый forensic reader для ext4.
 *
 * Его задача:
 *  - открыть userdata_decrypted.img
 *  - прочитать superblock
 *  - определить размер блока (1K/2K/4K)
 *  - предоставить чтение блоков по номеру
 *
 * Никаких mount, только прямой доступ к образу.
 */
class Ext4BlockDevice
{
public:
    explicit Ext4BlockDevice(const QString& imagePath);

    /**
     * @brief Открыть образ и загрузить superblock
     */
    bool open();

    /**
     * @brief Возвращает true если ext4 корректный
     */
    bool isValid() const;

    /**
     * @brief Размер блока файловой системы
     */
    quint32 blockSize() const;

    /**
     * @brief Читает один блок по номеру
     */
    QByteArray readBlock(quint64 blockNumber);

    /**
     * @brief Доступ к superblock
     */
    const Ext4Superblock& superblock() const;

private:
    QFile m_file;
    Ext4Superblock m_sb{};
    bool m_valid = false;
    quint32 m_blockSize = 4096;
	QByteArray readBytes(quint64 offset, quint64 size);
};
